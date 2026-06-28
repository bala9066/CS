// File hashing — ports ChecksumWorker::process from src/checksumworker.cpp.
// Computes MD5, SHA-1 (Node crypto) and CRC32 (ported table). Handles UNC paths.

import { createHash } from 'node:crypto';
import { createReadStream } from 'node:fs';
import { stat } from 'node:fs/promises';
import { crc32Hex } from './crc32.js';

export interface HashResult {
  success: boolean;
  md5: string;
  sha1: string;
  crc32: string;
  error?: string;
  sizeBytes?: number;
}

// Empty-file constants (match checksumworker.cpp).
const EMPTY_MD5 = 'd41d8cd98f00b204e9800998ecf8427e';
const EMPTY_SHA1 = 'da39a3ee5e6b4b0d3255bfef95601890afd80709';
const EMPTY_CRC32 = '00000000';

/**
 * Normalize a path the way ChecksumWorker does: resolve `file:` prefixes and
 * pick the right slash style for UNC vs local paths on Windows.
 */
export function normalizePath(rawPath: string): string {
  let cleanPath = (rawPath || '').trim();

  if (cleanPath.startsWith('file:')) {
    let rest = cleanPath.slice(5);
    while (rest.startsWith('/')) rest = rest.slice(1);
    cleanPath = rest ? '\\\\' + rest : rest;
  }

  const isUnc = cleanPath.startsWith('\\\\') || cleanPath.startsWith('//');
  if (isUnc) {
    cleanPath = cleanPath.replace(/\//g, '\\');
    if (!cleanPath.startsWith('\\\\')) cleanPath = '\\\\' + cleanPath.replace(/^\\+/, '');
  } else {
    cleanPath = cleanPath.replace(/\\/g, '/');
  }
  return cleanPath;
}

/** Hash a single file. Streams in chunks; computes all three digests in one pass. */
export async function hashFile(rawPath: string): Promise<HashResult> {
  const cleanPath = normalizePath(rawPath);

  let info;
  try {
    info = await stat(cleanPath);
  } catch {
    return { success: false, md5: '', sha1: '', crc32: '', error: `Cannot open file for reading: ${cleanPath}` };
  }

  if (info.isDirectory()) {
    return { success: false, md5: '', sha1: '', crc32: '', error: `Path resolves to a directory, not a file: ${cleanPath}` };
  }

  if (info.size === 0) {
    return { success: true, md5: EMPTY_MD5, sha1: EMPTY_SHA1, crc32: EMPTY_CRC32, sizeBytes: 0 };
  }

  return new Promise<HashResult>((resolve) => {
    const md5 = createHash('md5');
    const sha1 = createHash('sha1');
    // CRC32 running state (table-based, same as crc32.ts but incremental).
    let crc = 0xffffffff;
    const stream = createReadStream(cleanPath, { highWaterMark: 64 * 1024 });

    stream.on('data', (chunkRaw: Buffer | string) => {
      const chunk = chunkRaw as Buffer;
      md5.update(chunk);
      sha1.update(chunk);
      for (let i = 0; i < chunk.length; i++) {
        crc = (CRC_TABLE[(crc ^ chunk[i]) & 0xff] ^ (crc >>> 8)) >>> 0;
      }
    });
    stream.on('error', (err) => {
      resolve({ success: false, md5: '', sha1: '', crc32: '', error: `File read error: ${err.message}` });
    });
    stream.on('end', () => {
      crc = (crc ^ 0xffffffff) >>> 0;
      resolve({
        success: true,
        md5: md5.digest('hex'),
        sha1: sha1.digest('hex'),
        crc32: crc.toString(16).padStart(8, '0'),
        sizeBytes: info.size,
      });
    });
  });
}

// Local copy of the CRC table for incremental streaming (kept in sync with crc32.ts).
const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n >>> 0;
    for (let k = 0; k < 8; k++) c = c & 1 ? (0xedb88320 ^ (c >>> 1)) >>> 0 : (c >>> 1) >>> 0;
    table[n] = c >>> 0;
  }
  return table;
})();

// Re-export so callers can hash in-memory buffers too.
export { crc32Hex };
