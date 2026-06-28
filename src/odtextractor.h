#ifndef ODTEXTRACTOR_H
#define ODTEXTRACTOR_H

#include <QString>
#include <QByteArray>
#include <QVector>

// Minimal ZIP reader — extracts files from a ZIP archive into memory.
// ODT documents are ZIP archives containing content.xml with the document text.

// ZIP signature constants
constexpr quint32 ZIP_LOCAL_FILE_SIG = 0x04034b50;
constexpr quint32 ZIP_CENTRAL_DIR_SIG = 0x02014b50;
constexpr quint32 ZIP_EOCDR_SIG = 0x06054b50;
constexpr quint32 ZIP_EOCDR_MIN_SIZE = 22;
constexpr quint32 ZIP_LOCAL_HEADER_SIZE = 30;

// ODT entry name
constexpr char ODT_CONTENT_XML[] = "content.xml";

// Compression methods
constexpr quint16 ZIP_STORED = 0;
constexpr quint16 ZIP_DEFLATED = 8;

class OdtExtractor {
public:
    // Extract plain text from an ODT file. Returns empty on failure.
    static QString extractPlainText(const QString& filePath);

    // Check if a file is a valid readable ODT (ZIP archive with content.xml).
    static bool isValidOdt(const QString& filePath);

    // Build structured table-preserving text from ODT content.xml.
    static QString buildStructuredText(const QString& xml);

private:
    // --- Minimal ZIP reader (subset sufficient for ODT) ---
    struct LocalFileHeader {
        quint32 signature;
        quint16 versionNeeded;
        quint16 flags;
        quint16 compressionMethod;
        quint16 modTime;
        quint16 modDate;
        quint32 crc32;
        quint32 compressedSize;
        quint32 uncompressedSize;
        quint16 fileNameLen;
        quint16 extraFieldLen;
        quint16 commentLen;
        quint64 localHeaderOffset; // filled after scan
        QString fileName;
        QByteArray fileData;
    };

    struct CentralDirEntry {
        quint32 signature;
        quint16 versionMadeBy;
        quint16 versionNeeded;
        quint16 flags;
        quint16 compressionMethod;
        quint32 crc32;
        quint32 compressedSize;
        quint32 uncompressedSize;
        quint16 fileNameLen;
        quint16 extraFieldLen;
        quint16 commentLen;
        quint32 localHeaderOffset;
        QString fileName;
        QByteArray fileData;
    };

    static QVector<quint8> readFile(const QString& path);
    static bool readU16le(const QVector<quint8>& data, quint64 offset, quint16& out);
    static bool readU32le(const QVector<quint8>& data, quint64 offset, quint32& out);
    static QVector<CentralDirEntry> parseCentralDir(const QVector<quint8>& data);
    static QByteArray extractFileData(const QVector<quint8>& data, const CentralDirEntry& entry);
    static QVector<LocalFileHeader> parseLocalHeaders(const QVector<quint8>& data);
    static QByteArray inflateData(const QByteArray& compressed, quint32 uncompressedSize);
};

#endif // ODTEXTRACTOR_H
