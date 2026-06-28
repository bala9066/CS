#ifndef FOLDERSCANNER_H
#define FOLDERSCANNER_H

#include <QObject>
#include <QString>
#include <QStringList>

class FolderScanner : public QObject {
    Q_OBJECT
public:
    explicit FolderScanner(const QString& dirPath,
                           const QStringList& filters = {},
                           QObject* parent = nullptr);

public slots:
    void scan();

signals:
    void foundFiles(const QStringList& filePaths);
    void scanProgress(int filesFound);
    void finished();

private:
    QString m_dirPath;
    QStringList m_filters;
};

#endif // FOLDERSCANNER_H
