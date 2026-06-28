#ifndef CRC32_H
#define CRC32_H

#include <QtGlobal>

/**
 * @brief The CRC32 class provides a highly optimized checksum algorithm.
 * Uses IEEE 802.3 standard polynomial 0xEDB88320 with precomputed 256-word table.
 */
class CRC32 {
public:
    CRC32();
    void update(const void* data, qint64 length);
    quint32 result() const;
    void reset();

    static quint32 calculate(const void* data, qint64 length);

private:
    quint32 m_crc;
    static constexpr int TABLE_SIZE = 256;
    static const quint32 m_table[TABLE_SIZE];
};

#endif // CRC32_H
