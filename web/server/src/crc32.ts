// IEEE 802.3 standard CRC-32 (polynomial 0xEDB88320), ported 1:1 from src/crc32.cpp.
// Produces identical output to the Qt desktop app's CRC32 implementation.

const TABLE = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n >>> 0;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? (0xedb88320 ^ (c >>> 1)) >>> 0 : (c >>> 1) >>> 0;
    }
    table[n] = c >>> 0;
  }
  return table;
})();

/** Compute CRC-32 of a buffer, returned as lowercase 8-char zero-padded hex. */
export function crc32Hex(data: Buffer | Uint8Array): string {
  let crc = 0xffffffff;
  for (let i = 0; i < data.length; i++) {
    crc = (TABLE[(crc ^ data[i]) & 0xff] ^ (crc >>> 8)) >>> 0;
  }
  crc = (crc ^ 0xffffffff) >>> 0;
  return crc.toString(16).padStart(8, '0');
}
