#include "FileBrowserWidget.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>

static bool isAudioFile(const QString& path)
{
    static const QStringList exts = {
        QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("ogg"),
        QStringLiteral("mp3"), QStringLiteral("aiff")
    };
    return exts.contains(QFileInfo(path).suffix(), Qt::CaseInsensitive);
}

FileBrowserWidget::FileBrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    pathEdit_ = new QLineEdit(this);
    pathEdit_->setPlaceholderText(QStringLiteral("Drop an audio file here, or click Browse..."));
    pathEdit_->setReadOnly(true);
    layout->addWidget(pathEdit_, 1);

    browseButton_ = new QPushButton(QStringLiteral("Browse..."), this);
    layout->addWidget(browseButton_);

    connect(browseButton_, &QPushButton::clicked, this, &FileBrowserWidget::onBrowse);
}

QString FileBrowserWidget::filePath() const
{
    return pathEdit_->text();
}

void FileBrowserWidget::onBrowse()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Audio File"),
        QString(),
        QStringLiteral("Audio Files (*.wav *.flac *.ogg *.mp3 *.aiff);;All Files (*)"));
    if (!path.isEmpty())
        setPath(path);
}

void FileBrowserWidget::onPathEdited()
{
    // Only emitted from setPath which already validates
}

void FileBrowserWidget::setPath(const QString& path)
{
    pathEdit_->setText(path);
    pathEdit_->setToolTip(path);
    emit fileSelected(path);
}

void FileBrowserWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        const auto& urls = event->mimeData()->urls();
        if (urls.size() == 1 && urls.first().isLocalFile()
            && isAudioFile(urls.first().toLocalFile())) {
            event->acceptProposedAction();
        }
    }
}

void FileBrowserWidget::dropEvent(QDropEvent* event)
{
    const auto& urls = event->mimeData()->urls();
    if (!urls.empty()) {
        QString path = urls.first().toLocalFile();
        QFileInfo fi(path);
        if (fi.exists() && fi.isFile() && isAudioFile(path))
            setPath(path);
    }
}
