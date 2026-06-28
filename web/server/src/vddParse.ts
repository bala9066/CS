// VDD extraction prompt + LLM JSON -> VddRecord parsing.
// Ports the system prompt and parseLLMJsonResponse from src/mainwindow.cpp.

import type { VddRecord } from './types.js';
import { makeRecord } from './types.js';

export const VDD_SYSTEM_PROMPT =
  'You are a precise data extraction assistant. You will receive extracted text from a VDD (Version Description Document) file. The document contains tables listing configuration items (CIs) with their references, versions, and checksums.\n\n' +
  'CRITICAL INSTRUCTIONS:\n' +
  '1. EXTRACT EVERY SINGLE CI ITEM you can find in the tables. Count them carefully. Do NOT skip any, even if they do not have a checksum or if they have non-standard checksum values.\n' +
  '2. Each CI row has: File Name, CI Reference, Version, and optional checksum values.\n' +
  '3. Distinguish MD5 vs CRC-32 by length:\n' +
  '   - MD5 = exactly 32 hex characters (0-9, A-F) e.g., ABCDEF1234567890ABCDEF1234567890\n' +
  '   - CRC-32 = exactly 8 hex characters e.g., 12345678\n' +
  '   - 8-char values go in expectedCrc32, NOT expectedMd5\n' +
  '4. For each CI, output: fileName (with extension like .exe/.zip/.7z if available, or just the file name), ciReference, version, expectedMd5 (32 hex or empty), expectedCrc32 (8 hex or empty)\n' +
  '5. If a CI has only a CRC-32 (8-char) and no MD5, set expectedMd5 to empty string and expectedCrc32 to the 8-char value. If a CI has no checksum or a non-standard checksum, set both expectedMd5 and expectedCrc32 to empty strings.\n' +
  '6. Count the total number of CI items BEFORE outputting. If the document has 20 CIs, your JSON array must have exactly 20 elements.\n\n' +
  'Return ONLY a valid JSON array. No markdown, no explanation, no code fences, no backticks.\n\n' +
  'Example output (plain text, no backticks):\n' +
  '[{"fileName":"DP-CRF-xxx.exe","ciReference":"DP-CRF-xxx","version":"1V00","expectedMd5":"ABCDEF1234567890ABCDEF1234567890","expectedCrc32":"12345678"}]\n\n' +
  'If you find no CI items, return an empty array: []';

const HEX8 = /^[0-9a-fA-F]{8}$/;

/** Port of MainWindow::parseLLMJsonResponse. Returns parsed VDD records (id from 1). */
export function parseLLMJsonResponse(jsonString: string): VddRecord[] {
  const records: VddRecord[] = [];

  let cleaned = (jsonString || '').trim();
  if (cleaned.startsWith('```')) {
    // Drop a leading ```json / ``` fence and the trailing fence.
    cleaned = cleaned.replace(/^```[a-zA-Z]*\s*/, '');
    const endMark = cleaned.lastIndexOf('```');
    if (endMark !== -1) cleaned = cleaned.slice(0, endMark);
  }
  cleaned = cleaned.trim();

  let arr: any;
  try {
    arr = JSON.parse(cleaned);
  } catch {
    return records;
  }
  if (!Array.isArray(arr)) return records;

  let incId = 1;
  const seenCiRefs = new Set<string>();
  const seenFileNames = new Set<string>();

  for (const obj of arr) {
    if (!obj || typeof obj !== 'object') continue;
    const fn: string = obj.fileName ?? '';
    const ciRef: string = obj.ciReference ? obj.ciReference : fn;

    const normKey = ciRef.toLowerCase().trim();
    if (seenCiRefs.has(normKey)) continue;
    seenCiRefs.add(normKey);
    const normFn = fn.toLowerCase().trim();
    if (normFn && seenFileNames.has(normFn)) continue;
    if (normFn) seenFileNames.add(normFn);

    let expectedMd5: string = obj.expectedMd5 ?? '';
    let expectedCrc32: string = obj.expectedCrc32 ?? '';

    // 8-char value mistakenly in expectedMd5 is actually CRC32.
    if (expectedMd5 && expectedMd5.length === 8 && HEX8.test(expectedMd5)) {
      expectedCrc32 = expectedMd5;
      expectedMd5 = '';
    }
    if (!expectedCrc32) {
      const altCrc: string = obj.crc32 ?? '';
      if (altCrc.length === 8 && HEX8.test(altCrc)) expectedCrc32 = altCrc;
    }
    if (expectedMd5 && expectedMd5.length !== 32) expectedMd5 = '';

    if (!fn) continue;

    records.push(
      makeRecord({
        id: incId++,
        source: 'VDD',
        fileName: fn,
        ciReference: ciRef,
        version: obj.version ?? '',
        expectedMd5,
        expectedCrc32,
        localStatus: 'PENDING',
      })
    );
  }

  return records;
}
