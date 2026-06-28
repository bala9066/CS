#include "zipreader.h"

#include <QFile>
#include <zlib.h>
#include <QDebug>
#include <algorithm>

// ZIP signature constants
constexpr quint32 ZIP_LOCAL_FILE_SIG = 0x04034b50;
constexpr quint32 ZIP_CENTRAL_DIR_SIG = 0x02014b50;
constexpr quint32 ZIP_EOCDR_SIG = 0x06054b50;
constexpr quint32 ZIP_EOCDR_MIN_SIZE = 22;

static bool readU16le(const QVector<char>& data, quint64 offset, quint16& out)
{
    if (offset + 2 > data.size()) return false;
    out = static_cast<quint16>(static_cast<quint8>(data[offset]))
        | (static_cast<quint16>(static_cast<quint8>(data[offset + 1])) << 8);
    return true;
}

static bool readU32le(const QVector<char>& data, quint64 offset, quint32& out)
{
    if (offset + 4 > data.size()) return false;
    out = static_cast<quint32>(static_cast<quint8>(data[offset]))
        | (static_cast<quint32>(static_cast<quint8>(data[offset + 1])) << 8)
        | (static_cast<quint32>(static_cast<quint8>(data[offset + 2])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(data[offset + 3])) << 24);
    return true;
}

static QByteArray inflateRawDeflate(const QByteArray& compressed, quint32 uncompressedSize)
{
    if (uncompressedSize == 0)
        return {};

    QByteArray output;
    output.resize(static_cast<int>(uncompressedSize));

    z_stream stream{};
    // ZIP entries use raw deflate (no zlib/gzip wrapper). -15 = raw deflate.
    if (inflateInit2(&stream, -15) != Z_OK) {
        inflateEnd(&stream);
        return {};
    }

    stream.avail_in  = static_cast<uInt>(compressed.size());
    stream.next_in   = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.constData()));
    stream.avail_out = static_cast<uInt>(uncompressedSize);
    stream.next_out  = reinterpret_cast<Bytef*>(output.data());

    int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret != Z_STREAM_END && ret != Z_OK)
        return {};

    int actual = static_cast<int>(stream.total_out);
    if (actual > 0 && actual < output.size())
        output.truncate(actual);

    return output;
}

// ===========================================================================

ZipReader::ZipReader(const QString& filePath)
    : m_path(filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    m_rawData.resize(static_cast<int>(f.size()));
    qint64 bytes = f.read(m_rawData.data(), f.size());
    if (bytes < 0) {
        return;
    }
    m_rawData.resize(static_cast<int>(bytes));
    m_valid = isValid();
}

ZipReader::~ZipReader() = default;

bool ZipReader::exists() const { return m_valid; }

bool ZipReader::isReadable() const { return m_valid; }

QByteArray ZipReader::fileData(const QString& fileName) const
{
    for (const Entry& e : m_entries) {
        if (e.fileName == fileName) {
            // Read local file header to find exact offset of compressed data
            quint16 fnLen = 0, exLen = 0;
            if (!readU16le(m_rawData, e.offset + 26, fnLen) || !readU16le(m_rawData, e.offset + 28, exLen))
                return {};
            quint64 dataOffset = static_cast<quint64>(e.offset) + 30 + fnLen + exLen;

            if (e.compressionMethod == 0) {
                // Stored — return raw bytes directly
                if (dataOffset + e.compressedSize <= static_cast<quint64>(m_rawData.size()))
                    return QByteArray(m_rawData.constData() + static_cast<qsizetype>(dataOffset),
                                      static_cast<int>(e.compressedSize));
            } else if (e.compressionMethod == 8) {
                // Deflated — inflate
                QByteArray compressed;
                if (dataOffset + e.compressedSize <= static_cast<quint64>(m_rawData.size())) {
                    compressed = QByteArray(m_rawData.constData() + static_cast<qsizetype>(dataOffset),
                                            static_cast<int>(e.compressedSize));
                }
                return inflateRawDeflate(compressed, e.uncompressedSize);
            }
        }
    }
    return {};
}

bool ZipReader::isValid()
{
    if (static_cast<quint64>(m_rawData.size()) < ZIP_EOCDR_MIN_SIZE) return false;

    // Mutable copy for central directory parsing
    QVector<Entry> entries;

    // Find EOCDR from end
    quint32 sig = 0;
    for (qint64 i = static_cast<qint64>(m_rawData.size()) - ZIP_EOCDR_MIN_SIZE; i >= 0; --i) {
        if (readU32le(m_rawData, i, sig) && sig == ZIP_EOCDR_SIG) {
            // Found EOCDR — scan central directory entries
            quint16 commentLen = 0;
            if (!readU16le(m_rawData, i + ZIP_EOCDR_MIN_SIZE - 2, commentLen))
                return false;
            (void)commentLen;

            quint32 cdOffset = 0;
            if (!readU32le(m_rawData, i + ZIP_EOCDR_MIN_SIZE - 16, cdOffset))
                return false;

            quint16 cdEntryCount = 0;
            if (!readU16le(m_rawData, i + ZIP_EOCDR_MIN_SIZE - 10, cdEntryCount))
                return false;

            // Parse each central directory entry
            for (quint16 j = 0; j < cdEntryCount; ++j) {
                if (static_cast<quint64>(cdOffset) + 46 > static_cast<quint64>(m_rawData.size())) return false;

                quint32 cdSig = 0;
                if (!readU32le(m_rawData, cdOffset, cdSig) || cdSig != ZIP_CENTRAL_DIR_SIG)
                    return false;

                quint16 fnLen = 0, exLen = 0, cmLen = 0;
                if (!readU16le(m_rawData, cdOffset + 28, fnLen) ||
                    !readU16le(m_rawData, cdOffset + 30, exLen) ||
                    !readU16le(m_rawData, cdOffset + 32, cmLen)) {
                    return false;
                }

                Entry e;
                quint32 offVal = 0;
                quint16 cmVal = 0;
                quint32 crcVal = 0;
                quint32 csVal = 0;
                quint32 usVal = 0;
                if (!readU32le(m_rawData, cdOffset + 42, offVal) ||
                    !readU16le(m_rawData, cdOffset + 10, cmVal) ||
                    !readU32le(m_rawData, cdOffset + 16, crcVal) ||
                    !readU32le(m_rawData, cdOffset + 20, csVal) ||
                    !readU32le(m_rawData, cdOffset + 24, usVal)) {
                    return false;
                }
                e.offset = offVal;
                e.compressionMethod = cmVal;
                e.crc32 = crcVal;
                e.compressedSize = csVal;
                e.uncompressedSize = usVal;

                QByteArray rawName(m_rawData.constData() + static_cast<qsizetype>(cdOffset + 46), static_cast<int>(fnLen));
                e.fileName = QString::fromLatin1(rawName);

                entries.append(e);

                cdOffset += 46 + fnLen + exLen + cmLen;
            }
            m_entries = std::move(entries);
            return !entries.isEmpty();
        }
    }
    return false;
}
