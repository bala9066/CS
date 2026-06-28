#ifndef CHECKSUMWORKER_H
#define CHECKSUMWORKER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QCryptographicHash>
#include <atomic>

/**
 * @brief The ChecksumWorker class executes heavy-lifting file system parsing
 * and cryptographic hashing. Designed to run contextually inside QThread.
 */
class ChecksumWorker : public QObject {
    Q_OBJECT
public:
    explicit ChecksumWorker(int fileId, const QString& filePath, QObject* parent = nullptr);
    ~ChecksumWorker() override = default;

public slots:
    void process();
    void cancel();

signals:
    void started(int fileId);
    void fileProgress(int fileId, int percentage);
    void finished(int fileId, const QString& md5, const QString& sha1, const QString& crc32, bool success, const QString& errorStr);

private:
    int m_fileId;
    QString m_filePath;
    std::atomic<bool> m_isCancelled;
};

#endif // CHECKSUMWORKER_H
