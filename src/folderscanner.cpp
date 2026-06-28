#include "folderscanner.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QThread>

FolderScanner::FolderScanner(const QString& dirPath,
                             const QStringList& filters,
                             QObject* parent)
    : QObject(parent)
    , m_dirPath(dirPath)
    , m_filters(filters)
{
}

void FolderScanner::scan() {
    QDirIterator::IteratorFlags flags = QDirIterator::Subdirectories;
    // Emit files in batches so the UI can stay responsive even for huge directories
    const int batchSize = 1000;
    QStringList batch;
    batch.reserve(batchSize);
    int count = 0;

    if (m_filters.isEmpty()) {
        QDirIterator it(m_dirPath, QDir::Files, flags);
        while (it.hasNext()) {
            batch.append(it.next());
            ++count;
            if (count % batchSize == 0) {
                emit foundFiles(batch);
                batch.clear();
                emit scanProgress(count);
                QThread::msleep(5);
            }
        }
    } else {
        QDirIterator it(m_dirPath, m_filters, QDir::Files, flags);
        while (it.hasNext()) {
            batch.append(it.next());
            ++count;
            if (count % batchSize == 0) {
                emit foundFiles(batch);
                batch.clear();
                emit scanProgress(count);
                QThread::msleep(5);
            }
        }
    }

    // Emit any remaining files
    if (!batch.isEmpty()) {
        emit foundFiles(batch);
        batch.clear();
    }
    emit scanProgress(count);
    emit finished();
}
