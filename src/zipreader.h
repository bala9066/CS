#ifndef ZIPREADER_H
#define ZIPREADER_H

#include <QString>
#include <QByteArray>
#include <QVector>

/**
 * @brief Minimal ZIP reader — extracts files from any ZIP archive into memory.
 * Supports stored (uncompressed) and deflated (zlib) entries.
 */
class ZipReader {
public:
    explicit ZipReader(const QString& filePath);
    ~ZipReader();

    bool exists() const;
    bool isReadable() const;
    QByteArray fileData(const QString& fileName) const;

private:
    struct Entry {
        QString fileName;
        quint16 compressionMethod;
        quint32 crc32;
        quint32 compressedSize;
        quint32 uncompressedSize;
        quint64 offset; // offset of compressed data in the file
    };

    bool isValid();
    QVector<Entry> m_entries;
    QVector<char> m_rawData;
    QString m_path;
    bool m_valid = false;
};

#endif // ZIPREADER_H
