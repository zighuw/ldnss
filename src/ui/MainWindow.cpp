#include "MainWindow.h"
#include "FileBrowserWidget.h"
#include "AnalysisPanel.h"
#include "LoudnessChartWidget.h"
#include "WorkerController.h"
#include "TransportControls.h"
#include "WaveformWidget.h"
#include "LiveMeterPanel.h"
#include "playback/AudioPlayer.h"

#include <QCloseEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QKeySequence>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>
#include <QStatusBar>

#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ldnss — Audio Loudness Analyzer"));
    resize(960, 750);

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);

    // --- File browser (top) ---
    fileBrowser_ = new FileBrowserWidget(central);
    mainLayout->addWidget(fileBrowser_);

    // --- Transport controls ---
    transportControls_ = new TransportControls(central);
    mainLayout->addWidget(transportControls_);

    // --- Waveform (compact strip) ---
    waveformWidget_ = new WaveformWidget(central);
    mainLayout->addWidget(waveformWidget_);

    // --- Main area: meters | chart + analysis ---
    auto* mainSplitter = new QSplitter(Qt::Horizontal);

    liveMeterPanel_ = new LiveMeterPanel(mainSplitter);
    mainSplitter->addWidget(liveMeterPanel_);

    auto* rightSplitter = new QSplitter(Qt::Vertical);

    chartWidget_ = new LoudnessChartWidget(rightSplitter);
    rightSplitter->addWidget(chartWidget_);

    analysisPanel_ = new AnalysisPanel(rightSplitter);
    rightSplitter->addWidget(analysisPanel_);

    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 1);

    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 0); // meters: fixed width
    mainSplitter->setStretchFactor(1, 1); // chart+analysis: expand

    mainLayout->addWidget(mainSplitter, 1);

    setCentralWidget(central);

    createMenuBar();

    // --- Worker controller (invisible) ---
    workerController_ = new WorkerController(this);

    // --- Audio player (invisible) ---
    audioPlayer_ = new AudioPlayer(this);

    // --- Signal wiring ---

    // File selection
    connect(fileBrowser_, &FileBrowserWidget::fileSelected,
            this, &MainWindow::onFileSelected);

    // Offline analysis
    connect(workerController_, &WorkerController::analysisProgress,
            analysisPanel_, &AnalysisPanel::showProgress);

    connect(workerController_, &WorkerController::analysisComplete,
            analysisPanel_, &AnalysisPanel::showResult);

    connect(workerController_, &WorkerController::analysisComplete,
            chartWidget_, &LoudnessChartWidget::showResult);

    connect(workerController_, &WorkerController::analysisError,
            analysisPanel_, &AnalysisPanel::showError);

    // Transport → AudioPlayer
    connect(transportControls_, &TransportControls::playRequested,
            audioPlayer_, &AudioPlayer::play);

    connect(transportControls_, &TransportControls::pauseRequested,
            audioPlayer_, &AudioPlayer::pause);

    connect(transportControls_, &TransportControls::stopRequested,
            audioPlayer_, &AudioPlayer::stop);

    connect(transportControls_, &TransportControls::seekRequested,
            this, [this](float seconds) {
                const auto& fmt = audioPlayer_->format();
                int64_t frame = static_cast<int64_t>(seconds * fmt.sampleRate);
                audioPlayer_->seek(frame);
            });

    // AudioPlayer → UI
    connect(audioPlayer_, &AudioPlayer::stateChanged,
            transportControls_, &TransportControls::setState);

    connect(audioPlayer_, &AudioPlayer::positionChanged,
            transportControls_, &TransportControls::setPosition);

    connect(audioPlayer_, &AudioPlayer::positionChanged,
            waveformWidget_, [this](int64_t /*frame*/, float seconds) {
                waveformWidget_->setPlaybackPosition(seconds);
            });

    connect(audioPlayer_, &AudioPlayer::errorOccurred,
            this, &MainWindow::onPlayerError);

    // Waveform seek
    connect(waveformWidget_, &WaveformWidget::seekRequested,
            this, [this](float seconds) {
                const auto& fmt = audioPlayer_->format();
                int64_t frame = static_cast<int64_t>(seconds * fmt.sampleRate);
                audioPlayer_->seek(frame);
            });

    // Meter reset
    connect(transportControls_, &TransportControls::resetMetersRequested,
            this, [this]() {
                audioPlayer_->resetMeter();
                liveMeterPanel_->reset();
            });

    // Live meter panel
    liveMeterPanel_->setPlayer(audioPlayer_);

    restoreGeometrySettings();
    loadRecentFiles();

    statusBar()->showMessage(QStringLiteral("Ready"));
}

MainWindow::~MainWindow() = default;

void MainWindow::onFileSelected(const QString& path)
{
    if (workerController_->isRunning()) {
        statusBar()->showMessage(
            QStringLiteral("Already analyzing — wait or cancel before opening another file"));
        return;
    }

    addRecentFile(path);

    // Stop any previous playback
    stopPlaybackAndReset();

    // Load file for playback / metering
    std::string pathStr = path.toStdString();
    if (!audioPlayer_->load(pathStr)) {
        analysisPanel_->clear();
        chartWidget_->clear();
        workerController_->startAnalysis(path);
        return;
    }

    // Set up metering widgets
    waveformWidget_->load(pathStr);
    transportControls_->setTotalDuration(waveformWidget_->totalDuration());
    liveMeterPanel_->reset();

    // Start offline analysis
    analysisPanel_->clear();
    chartWidget_->clear();
    statusBar()->showMessage(QStringLiteral("Analyzing %1...").arg(path));
    workerController_->startAnalysis(path);
}

void MainWindow::onPlayerError(const QString& msg)
{
    statusBar()->showMessage(msg);
}

void MainWindow::stopPlaybackAndReset()
{
    audioPlayer_->stop();
    waveformWidget_->clear();
    transportControls_->setTotalDuration(0.0f);
    liveMeterPanel_->reset();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveGeometrySettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::createMenuBar()
{
    fileMenu_ = menuBar()->addMenu(QStringLiteral("&File"));

    openAction_ = fileMenu_->addAction(QStringLiteral("&Open..."));
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, fileBrowser_, &FileBrowserWidget::onBrowse);

    fileMenu_->addSeparator();

    recentFilesMenu_ = fileMenu_->addMenu(QStringLiteral("Recent &Files"));
    clearRecentAction_ = recentFilesMenu_->addAction(QStringLiteral("Clear Recent Files"));
    clearRecentAction_->setEnabled(false);
    connect(clearRecentAction_, &QAction::triggered, this, [this]() {
        recentFiles_.clear();
        QSettings settings(QStringLiteral("ldnss"), QStringLiteral("ldnss"));
        settings.beginGroup(QStringLiteral("recentFiles"));
        settings.remove(QString());
        settings.endGroup();
        updateRecentFilesMenu();
    });

    fileMenu_->addSeparator();

    quitAction_ = fileMenu_->addAction(QStringLiteral("&Quit"));
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &QMainWindow::close);
}

void MainWindow::addRecentFile(const QString& path)
{
    // Dedup — remove if already present
    recentFiles_.removeAll(path);
    // Remove from QSettings list as well
    QFileInfo fi(path);
    QString canonical = fi.absoluteFilePath();
    recentFiles_.removeAll(canonical);

    recentFiles_.prepend(canonical);
    while (recentFiles_.size() > kMaxRecentFiles)
        recentFiles_.removeLast();

    // Persist
    QSettings settings(QStringLiteral("ldnss"), QStringLiteral("ldnss"));
    settings.beginGroup(QStringLiteral("recentFiles"));
    settings.setValue(QStringLiteral("size"), recentFiles_.size());
    for (int i = 0; i < recentFiles_.size(); ++i)
        settings.setValue(QString::number(i), recentFiles_[i]);
    settings.endGroup();

    updateRecentFilesMenu();
}

void MainWindow::loadRecentFiles()
{
    QSettings settings(QStringLiteral("ldnss"), QStringLiteral("ldnss"));
    settings.beginGroup(QStringLiteral("recentFiles"));
    int size = settings.value(QStringLiteral("size"), 0).toInt();
    recentFiles_.clear();
    for (int i = 0; i < size && i < kMaxRecentFiles; ++i) {
        QString key = QString::number(i);
        if (settings.contains(key))
            recentFiles_.append(settings.value(key).toString());
    }
    settings.endGroup();
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    // Remove all existing file actions (keep only "Clear Recent Files")
    const auto& actions = recentFilesMenu_->actions();
    for (QAction* a : actions) {
        if (a != clearRecentAction_)
            recentFilesMenu_->removeAction(a);
    }

    for (int i = 0; i < recentFiles_.size(); ++i) {
        const QString& path = recentFiles_[i];
        QAction* action = recentFilesMenu_->addAction(
            QStringLiteral("&%1 %2").arg(i + 1).arg(path));
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, [this, path]() {
            onFileSelected(path);
        });
    }

    clearRecentAction_->setEnabled(!recentFiles_.empty());
}

void MainWindow::saveGeometrySettings()
{
    QSettings settings(QStringLiteral("ldnss"), QStringLiteral("ldnss"));
    settings.beginGroup(QStringLiteral("window"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.endGroup();
}

void MainWindow::restoreGeometrySettings()
{
    QSettings settings(QStringLiteral("ldnss"), QStringLiteral("ldnss"));
    settings.beginGroup(QStringLiteral("window"));
    QByteArray geo = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
    else
        resize(960, 750);
    settings.endGroup();
}
