#pragma once

#include <QMainWindow>

class FileBrowserWidget;
class AnalysisPanel;
class LoudnessChartWidget;
class WorkerController;
class TransportControls;
class WaveformWidget;
class LiveMeterPanel;
class AudioPlayer;

class QMenu;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onFileSelected(const QString& path);
    void onPlayerError(const QString& msg);

private:
    void stopPlaybackAndReset();

    // File menu + recent files
    void createMenuBar();
    void addRecentFile(const QString& path);
    void loadRecentFiles();
    void updateRecentFilesMenu();
    void saveGeometrySettings();
    void restoreGeometrySettings();

    FileBrowserWidget* fileBrowser_;
    TransportControls* transportControls_;
    WaveformWidget* waveformWidget_;
    LiveMeterPanel* liveMeterPanel_;
    LoudnessChartWidget* chartWidget_;
    AnalysisPanel* analysisPanel_;

    WorkerController* workerController_;
    AudioPlayer* audioPlayer_;

    // File menu
    QMenu* fileMenu_ = nullptr;
    QMenu* recentFilesMenu_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* clearRecentAction_ = nullptr;
    QAction* quitAction_ = nullptr;

    QStringList recentFiles_;
    static constexpr int kMaxRecentFiles = 10;
};
