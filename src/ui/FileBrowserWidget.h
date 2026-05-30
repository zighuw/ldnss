#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;

class FileBrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit FileBrowserWidget(QWidget* parent = nullptr);

    QString filePath() const;

public slots:
    void onBrowse();

signals:
    void fileSelected(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onPathEdited();

private:
    void setPath(const QString& path);

    QLineEdit* pathEdit_;
    QPushButton* browseButton_;
};
