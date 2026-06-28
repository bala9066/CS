#include "odtextractor.h"

#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <zlib.h>

// ===========================================================================
// Minimal ZIP reader — extracts files from a ZIP archive into memory.
// ODT documents are ZIP archives containing content.xml with the document text.
// ===========================================================================

bool OdtExtractor::readU16le(const QVector<quint8>& data, quint64 offset, quint16& out)
{
    if (offset + 2 > static_cast<quint64>(data.size())) return false;
    out = static_cast<quint16>(data[offset])
        | (static_cast<quint16>(data[offset + 1]) << 8);
    return true;
}

bool OdtExtractor::readU32le(const QVector<quint8>& data, quint64 offset, quint32& out)
{
    if (offset + 4 > static_cast<quint64>(data.size())) return false;
    out = static_cast<quint32>(data[offset])
        | (static_cast<quint32>(data[offset + 1]) << 8)
        | (static_cast<quint32>(data[offset + 2]) << 16)
        | (static_cast<quint32>(data[offset + 3]) << 24);
    return true;
}

static QByteArray inflateRawDeflateFallback(const QByteArray& compressed);

static QByteArray inflateRawDeflate(const QByteArray& compressed, quint32 uncompressedSize)
{
    if (uncompressedSize == 0)
        return {};

    QByteArray output;
    output.resize(static_cast<int>(uncompressedSize));

    z_stream stream{};
    // -15 = raw deflate (no zlib/gzip wrapper), required for ZIP entries.
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
// Central directory parsing — scans entire file for PK\x01\x02 signatures.
// Works even when EOCDR is missing or corrupted.
// ===========================================================================

QVector<OdtExtractor::CentralDirEntry> OdtExtractor::parseCentralDir(const QVector<quint8>& data)
{
    QVector<CentralDirEntry> entries;
    const quint32 cdSig = ZIP_CENTRAL_DIR_SIG;

    for (quint64 cdPos = 0; cdPos + 46 <= static_cast<quint64>(data.size());) {
        quint32 entrySig = 0;
        if (!readU32le(data, cdPos, entrySig) || entrySig != cdSig) {
            ++cdPos;
            continue;
        }

        quint16 fnLen = 0, exLen = 0, cmLen = 0;
        if (!readU16le(data, cdPos + 28, fnLen) ||
            !readU16le(data, cdPos + 30, exLen) ||
            !readU16le(data, cdPos + 32, cmLen)) {
            ++cdPos;
            continue;
        }

        CentralDirEntry entry;
        if (!readU32le(data, cdPos + 42, entry.localHeaderOffset) ||
            !readU32le(data, cdPos + 16, entry.crc32) ||
            !readU32le(data, cdPos + 20, entry.compressedSize) ||
            !readU32le(data, cdPos + 24, entry.uncompressedSize)) {
            ++cdPos;
            continue;
        }

        // Read compression method (CD offset +10)
        readU16le(data, cdPos + 10, entry.compressionMethod);

        QByteArray rawName(reinterpret_cast<const char*>(data.constData() + static_cast<qsizetype>(cdPos + 46)), static_cast<int>(fnLen));
        entry.fileName = QString::fromLatin1(rawName);

        // Store the raw compressed data from central directory if stored
        if (entry.compressionMethod == ZIP_STORED && entry.compressedSize > 0) {
            quint64 dataStart = cdPos + 46 + fnLen + exLen + cmLen;
            if (dataStart + entry.compressedSize <= static_cast<quint64>(data.size())) {
                QByteArray raw(reinterpret_cast<const char*>(data.constData() + static_cast<qsizetype>(dataStart)),
                               static_cast<int>(entry.compressedSize));
                entry.fileData = raw;
            }
        }

        entries.append(entry);
        cdPos += 46 + fnLen + exLen + cmLen;
    }
    return entries;
}

// ===========================================================================
// Extract a single file's data using central directory info.
// Reads compressed data from the local file header offset.
// ===========================================================================

QByteArray OdtExtractor::extractFileData(const QVector<quint8>& data, const CentralDirEntry& entry)
{
    if (entry.fileName.isEmpty())
        return {};

    // Validate we have size info
    if (entry.compressedSize == 0 && entry.uncompressedSize == 0)
        return {};

    quint64 scanStart = entry.localHeaderOffset;

    // If localHeaderOffset is zero, scan entire file for the matching local file header
    if (scanStart == 0)
        scanStart = 0;

    // Scan for the matching local file header by filename
    quint32 localSig = ZIP_LOCAL_FILE_SIG;
    for (quint64 pos = scanStart; pos + 30 <= static_cast<quint64>(data.size());) {
        quint32 sig = 0;
        if (!readU32le(data, pos, sig) || sig != localSig) {
            ++pos;
            continue;
        }

        quint16 fnLen = 0, exLen = 0;
        if (!readU16le(data, pos + 26, fnLen) || !readU16le(data, pos + 28, exLen)) {
            ++pos;
            continue;
        }

        QByteArray rawName(reinterpret_cast<const char*>(data.constData() + static_cast<qsizetype>(pos + 30)),
                           static_cast<int>(fnLen));
        if (QString::fromLatin1(rawName) == entry.fileName) {
            // Found matching local header
            quint64 dataOffset = pos + 30 + fnLen + exLen;
            quint32 compSize = entry.compressedSize;
            if (compSize == 0) {
                // Data descriptor — compressed size not in header.
                // Use uncompressed size as an upper bound, then try to decompress.
                if (entry.uncompressedSize == 0)
                    return {};
                compSize = static_cast<quint32>(entry.uncompressedSize);
            }

            if (dataOffset + compSize > static_cast<quint64>(data.size())) {
                ++pos;
                continue;
            }

            QByteArray compressed(reinterpret_cast<const char*>(data.constData() + static_cast<qsizetype>(dataOffset)),
                                  static_cast<int>(compSize));

            if (entry.compressionMethod == ZIP_STORED)
                return compressed;

            if (entry.compressionMethod == ZIP_DEFLATED) {
                QByteArray inflated = inflateRawDeflate(compressed, entry.uncompressedSize);
                if (!inflated.isEmpty())
                    return inflated;
                return inflateRawDeflateFallback(compressed);
            }

            return {};
        }

        // Not a match — skip past this entry
        quint64 next = pos + 30 + fnLen + exLen;
        // If we have a valid size, skip over the data; otherwise advance carefully
        if (entry.compressedSize > 0 && next + entry.compressedSize > static_cast<quint64>(data.size())) {
            ++pos;
        } else {
            pos = next;
        }
    }

    return {};
}

static QByteArray inflateRawDeflateFallback(const QByteArray& compressed)
{
    if (compressed.isEmpty())
        return {};

    // Try progressively larger output buffers
    for (int exp = 16; exp <= 26; ++exp) {
        quint32 trySize = 1u << exp; // 64KB .. 64MB
        QByteArray output;
        output.resize(static_cast<int>(trySize));

        z_stream stream{};
        if (inflateInit2(&stream, -15) != Z_OK) {
            inflateEnd(&stream);
            continue;
        }

        stream.avail_in  = static_cast<uInt>(compressed.size());
        stream.next_in   = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.constData()));
        stream.avail_out = static_cast<uInt>(trySize);
        stream.next_out  = reinterpret_cast<Bytef*>(output.data());

        int ret = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);

        if (ret == Z_STREAM_END) {
            int actual = static_cast<int>(stream.total_out);
            if (actual > 0 && actual < static_cast<int>(trySize))
                output.truncate(actual);
            return output;
        }
        if (ret == Z_OK && stream.total_out > 0) {
            // Output was too small — try next larger
            continue;
        }
        // Z_BUF_ERROR or other failure with this size — try next
    }
    return {};
}

// ===========================================================================

QVector<quint8> OdtExtractor::readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[OdtExtractor] Cannot open file:" << path;
        return {};
    }
    QVector<quint8> data;
    data.resize(static_cast<int>(f.size()));
    qint64 bytesRead = f.read(reinterpret_cast<char*>(data.data()), f.size());
    if (bytesRead < 0) {
        qWarning() << "[OdtExtractor] Read error:" << f.errorString();
        return {};
    }
    data.resize(static_cast<int>(bytesRead));
    return data;
}

bool OdtExtractor::isValidOdt(const QString& filePath)
{
    QVector<quint8> data = readFile(filePath);
    if (data.isEmpty()) return false;
    if (static_cast<quint64>(data.size()) < 4) return false;

    // Check ZIP magic at start
    quint32 startSig = 0;
    if (!readU32le(data, 0, startSig) || startSig != ZIP_LOCAL_FILE_SIG)
        return false;

    // Must have a central directory (at least one PK\x01\x02 entry)
    auto cdEntries = parseCentralDir(data);
    return !cdEntries.isEmpty();
}

QByteArray OdtExtractor::inflateData(const QByteArray& compressed, quint32 uncompressedSize)
{
    return ::inflateRawDeflate(compressed, uncompressedSize);
}

QString OdtExtractor::extractPlainText(const QString& filePath)
{
    QVector<quint8> data = readFile(filePath);
    if (data.isEmpty()) return {};

    // Parse central directory
    auto cdEntries = parseCentralDir(data);

    // Find content.xml entry in central directory
    for (const auto& e : cdEntries) {
        if (e.fileName == ODT_CONTENT_XML) {
            // If stored in central directory, use it directly
            if (!e.fileData.isEmpty()) {
                QString xml = QString::fromUtf8(e.fileData);
                return buildStructuredText(xml);
            }
            // Otherwise read from local file header and decompress
            QByteArray fileData = extractFileData(data, e);
            if (!fileData.isEmpty()) {
                QString xml = QString::fromUtf8(fileData);
                return buildStructuredText(xml);
            }
        }
    }

    return {};
}

QString OdtExtractor::buildStructuredText(const QString& xml)
{
    // Build structured text preserving table structure for LLM consumption.
    // The ODT document contains many tables, each representing a CI item with rows
    // for File Name, CI Reference, Version, Checksum, etc.
    //
    // Strategy: Walk the XML tree, detect tables, and for each table output
    // rows in a structured format: each row as "cell1 | cell2 | cell3" on its own line.
    // This preserves the relationship between labels and values.

    QString text = xml;

    // 1. Normalize invisible Unicode characters to regular spaces
    text.replace(QRegularExpression("[​-‏‪-‮⁠﻿­ ]"), "");

    // 2. For each table, extract structured rows using namespaced tags.
    // Process tables one at a time, extracting cells into "label | value" pairs.
    // Match only a real <table:table ...> open (next char is a space or '>') so the
    // depth counter is not thrown off by <table:table-row/-cell/-column> children,
    // which share the "<table:table" prefix. (Previously indexOf("<table:table")
    // matched those children, so depth never returned to 0 and tableEnd stayed -1,
    // silently falling back to flat, unstructured text.)
    auto findTableOpen = [&text](int from) -> int {
        int p = from;
        for (;;) {
            int idx = text.indexOf("<table:table", p);
            if (idx == -1) return -1;
            QChar next = (idx + 12 < text.length()) ? text.at(idx + 12) : QChar();
            if (next == ' ' || next == '>') return idx;
            p = idx + 12;
        }
    };

    while (findTableOpen(0) != -1) {
        int tableStart = findTableOpen(0);
        if (tableStart == -1) break;

        // Find the matching closing tag while tracking nesting depth so that
        // inner (nested) tables do not prematurely terminate the outer table.
        int tableEnd = -1;
        {
            int depth = 1;
            int searchPos = tableStart + 12; // skip past "<table:table"
            while (searchPos < text.length() && depth > 0) {
                int nextOpen  = findTableOpen(searchPos);
                int nextClose = text.indexOf("</table:table>", searchPos);
                if (nextClose == -1) break;
                if (nextOpen != -1 && nextOpen < nextClose) {
                    depth++;
                    searchPos = nextOpen + 12;
                } else {
                    depth--;
                    if (depth == 0) tableEnd = nextClose;
                    searchPos = nextClose + 14;
                }
            }
        }
        if (tableEnd == -1) break;

        QString tableContent = text.mid(tableStart, tableEnd - tableStart + 14);

        // Extract rows from this table
        QString rowText;
        int pos = 0;
        while (pos < tableContent.length()) {
            int rowStart = tableContent.indexOf("<table:table-row", pos);
            if (rowStart == -1) break;
            int rowEnd = tableContent.indexOf("</table:table-row>", rowStart);
            if (rowEnd == -1) break;

            QString rowContent = tableContent.mid(rowStart, rowEnd - rowStart + 18);

            // Extract cells
            QString cellText;
            int cellPos = 0;
            int cellCount = 0;
            while (cellPos < rowContent.length()) {
                int cellStart = rowContent.indexOf("<table:table-cell", cellPos);
                int cellEnd = -1;
                int cellLen = 19; // length of </table:table-cell>

                if (cellStart == -1) {
                    cellStart = rowContent.indexOf("<table:covered-table-cell", cellPos);
                    if (cellStart == -1) break;
                    cellEnd = rowContent.indexOf("</table:covered-table-cell>", cellStart);
                    cellLen = 27; // length of </table:covered-table-cell>
                } else {
                    cellEnd = rowContent.indexOf("</table:table-cell>", cellStart);
                }

                if (cellEnd == -1) break;

                QString cellContent = rowContent.mid(cellStart, cellEnd - cellStart + cellLen);
                // Strip all XML tags to get pure text
                QString cellPlain = cellContent;
                cellPlain.replace(QRegularExpression("<[^>]*>"), " ");
                cellPlain = cellPlain.trimmed();
                if (!cellPlain.isEmpty()) {
                    if (cellCount > 0) cellText += " | ";
                    cellText += cellPlain;
                    cellCount++;
                }
                cellPos = cellEnd + cellLen;
            }

            if (!cellText.isEmpty() && !rowText.isEmpty()) rowText += "\n";
            if (!cellText.isEmpty()) rowText += cellText;

            pos = rowEnd + 18;
        }

        if (!rowText.isEmpty()) {
            text.replace(tableStart, tableEnd - tableStart + 14, "\n---TABLE---\n" + rowText + "\n---TABLE---\n");
        } else {
            text.replace(tableStart, tableEnd - tableStart + 14, "\n---TABLE---\n(Empty table)\n---TABLE---\n");
        }
    }

    // 3. Strip XML namespace prefixes only from the non-table (XML) sections of the
    //    text.  The table content inside ---TABLE--- markers is already plain text;
    //    applying the regex there would corrupt values such as "http://..." or any
    //    field containing a lowercase word followed by a colon.
    {
        const QString marker = "---TABLE---";
        QStringList parts = text.split(marker);
        // Even-indexed parts are XML content (between/around tables); odd-indexed
        // parts are the extracted table rows — leave those untouched.
        for (int i = 0; i < parts.size(); i += 2)
            parts[i].replace(QRegularExpression("([a-z]+):"), "");
        text = parts.join(marker);
    }

    // 4. Strip all remaining XML tags
    text.replace(QRegularExpression("<[^>]*>"), " ");

    // 5. Normalize multiple spaces to single space but preserve newlines
    text.replace(QRegularExpression(" {2,}"), " ");

    return text.trimmed();
}
