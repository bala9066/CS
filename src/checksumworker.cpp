#include "checksumworker.h"
#include "crc32.h"
#include <QFileInfo>
#include <QThread>
#include <QDebug>

ChecksumWorker::ChecksumWorker(int fileId, const QString& filePath, QObject* parent)
    : QObject(parent)
    , m_fileId(fileId)
    , m_filePath(filePath)
    , m_isCancelled(false)
{
}

void ChecksumWorker::cancel() {
    m_isCancelled = true;
}

void ChecksumWorker::process() {
    emit started(m_fileId);

    QString cleanPath = m_filePath.trimmed();

    // Handle "file:" prefixed paths (e.g., file://10.5.0.10/... or file:///server/share/...)
    if (cleanPath.startsWith("file:")) {
        QString rest = cleanPath.mid(5);
        while (rest.startsWith('/')) {
            rest = rest.mid(1);
        }
        if (!rest.isEmpty()) {
            cleanPath = "\\\\" + rest;
        } else {
            cleanPath = rest;
        }
    }

    // On Windows with MinGW, QFile does NOT reliably open UNC paths with forward slashes.
    // UNC paths must use backslashes (\\server\share\...).
    bool isUnc = cleanPath.startsWith("\\\\") || cleanPath.startsWith("//");
    if (isUnc) {
        cleanPath.replace("/", "\\");
    } else {
        cleanPath.replace("\\", "/");
    }

    // Validate: if it resolves to a directory, reject immediately
    if (QFileInfo(cleanPath).isDir()) {
        emit finished(m_fileId, "", "", "", false, "Path resolves to a directory, not a file: " + cleanPath);
        return;
    }

    QFile file(cleanPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QString errorMsg = QString("Cannot open file for reading: %1").arg(cleanPath);
        emit finished(m_fileId, "", "", "", false, errorMsg);
        return;
    }

    qint64 totalSize = file.size();
    if (totalSize == 0) {
        emit fileProgress(m_fileId, 100);
        emit finished(m_fileId, 
                     "d41d8cd98f00b204e9800998ecf8427e", // standard empty string MD5
                     "da39a3ee5e6b4b0d3255bfef95601890afd80709", // standard empty string SHA-1
                     "00000000", // empty CRC32
                     true, "");
        return;
    }

    QCryptographicHash md5Hash(QCryptographicHash::Md5);
    QCryptographicHash sha1Hash(QCryptographicHash::Sha1);
    CRC32 crcCalculator;

    const qint64 chunkSize = 64 * 1024; // 64 KB buffer size reads for high IO performance
    QByteArray buffer;
    buffer.resize(chunkSize);

    qint64 bytesReadTotal = 0;
    int lastPercent = 0;

    while (bytesReadTotal < totalSize) {
        if (m_isCancelled) {
            emit finished(m_fileId, "", "", "", false, "Process cancelled by user.");
            return;
        }

        qint64 read = file.read(buffer.data(), chunkSize);
        if (read < 0) {
            emit finished(m_fileId, "", "", "", false, "File read error encountered intermediate.");
            return;
        }
        if (read == 0) {
            break;
        }

        md5Hash.addData(buffer.constData(), read);
        sha1Hash.addData(buffer.constData(), read);
        crcCalculator.update(buffer.constData(), read);

        bytesReadTotal += read;
        
        int percent = static_cast<int>((bytesReadTotal * 100) / totalSize);
        if (percent > lastPercent) {
            lastPercent = percent;
            emit fileProgress(m_fileId, percent);
        }

        // Slight yield to guarantee UI responsiveness for huge single-file hashing
        QThread::msleep(2);
    }

    file.close();

    QString md5Hex = QString::fromLatin1(md5Hash.result().toHex());
    QString sha1Hex = QString::fromLatin1(sha1Hash.result().toHex());

    quint32 finalCrc = crcCalculator.result();
    QString crcHex = QString("%1").arg(finalCrc, 8, 16, QChar('0'));


    emit fileProgress(m_fileId, 100);
    emit finished(m_fileId, md5Hex, sha1Hex, crcHex, true, "");
}
