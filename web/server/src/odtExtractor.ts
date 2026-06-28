// ODT plain-text extraction. ODT files are ZIP archives containing content.xml.
// ZIP layer uses fflate; buildStructuredText() is based on src/odtextractor.cpp's
// buildStructuredText, with a corrected table-boundary scan so the structured
// "cell | cell" rows (wrapped in ---TABLE--- markers) are actually produced — the
// C++ version's indexOf("<table:table") also matched <table:table-row/-cell/-column>
// and so its depth counter never returned to zero (it silently fell back to flat
// text). Producing real structure makes LLM CI extraction more reliable.

import { unzipSync } from 'fflate';

const CONTENT_XML = 'content.xml';

/** True if the buffer is a ZIP that contains content.xml (an ODT). */
export function isValidOdt(buf: Buffer | Uint8Array): boolean {
  if (!buf || buf.length < 4) return false;
  // ZIP local file header signature: 0x04034b50 (PK\x03\x04)
  if (!(buf[0] === 0x50 && buf[1] === 0x4b && buf[2] === 0x03 && buf[3] === 0x04)) return false;
  try {
    const files = unzipSync(buf as Uint8Array, { filter: (f) => f.name === CONTENT_XML });
    return !!files[CONTENT_XML];
  } catch {
    return false;
  }
}

/** Extract structured plain text from an ODT buffer. Returns '' on failure. */
export function extractPlainText(buf: Buffer | Uint8Array): string {
  try {
    const files = unzipSync(buf as Uint8Array, { filter: (f) => f.name === CONTENT_XML });
    const content = files[CONTENT_XML];
    if (!content) return '';
    const xml = Buffer.from(content).toString('utf8');
    return buildStructuredText(xml);
  } catch {
    return '';
  }
}

/**
 * Turn each table into `cell | cell` rows wrapped in ---TABLE--- markers, then
 * strip remaining tags. Preserves the label/value relationships in CI tables.
 */
export function buildStructuredText(xml: string): string {
  let text = xml;

  // 1. Strip invisible Unicode chars (zero-width U+200B-U+200F, directional marks
  //    U+202A-U+202E, word joiner U+2060, BOM U+FEFF, soft hyphen U+00AD,
  //    non-breaking space U+00A0). Explicit escapes so a regular space (U+0020) is
  //    never accidentally included.
  text = text.replace(/[​-‏‪-‮⁠﻿­ ]/g, '');

  // Find the next *real* <table:table ...> open (followed by a space or '>'), not
  // the <table:table-row/-cell/-column> children that share the prefix.
  const findTableOpen = (from: number): number => {
    let p = from;
    for (;;) {
      const idx = text.indexOf('<table:table', p);
      if (idx === -1) return -1;
      const next = text[idx + 12];
      if (next === ' ' || next === '>') return idx;
      p = idx + 12;
    }
  };

  // 2. Process each table into structured rows.
  while (findTableOpen(0) !== -1) {
    const tableStart = findTableOpen(0);
    if (tableStart === -1) break;

    // Find matching closing tag, tracking nesting depth (only real nested tables).
    let tableEnd = -1;
    {
      let depth = 1;
      let searchPos = tableStart + 12;
      while (searchPos < text.length && depth > 0) {
        const nextOpen = findTableOpen(searchPos);
        const nextClose = text.indexOf('</table:table>', searchPos);
        if (nextClose === -1) break;
        if (nextOpen !== -1 && nextOpen < nextClose) {
          depth++;
          searchPos = nextOpen + 12;
        } else {
          depth--;
          if (depth === 0) tableEnd = nextClose;
          searchPos = nextClose + 14;
        }
      }
    }
    if (tableEnd === -1) break;

    const tableContent = text.substring(tableStart, tableEnd + 14);

    // Extract rows.
    let rowText = '';
    let pos = 0;
    while (pos < tableContent.length) {
      const rowStart = tableContent.indexOf('<table:table-row', pos);
      if (rowStart === -1) break;
      const rowEnd = tableContent.indexOf('</table:table-row>', rowStart);
      if (rowEnd === -1) break;

      const rowContent = tableContent.substring(rowStart, rowEnd + 18);

      // Extract cells.
      let cellText = '';
      let cellPos = 0;
      let cellCount = 0;
      while (cellPos < rowContent.length) {
        let cellStart = rowContent.indexOf('<table:table-cell', cellPos);
        let cellEnd = -1;
        let cellLen = 19; // length of </table:table-cell>

        if (cellStart === -1) {
          cellStart = rowContent.indexOf('<table:covered-table-cell', cellPos);
          if (cellStart === -1) break;
          cellEnd = rowContent.indexOf('</table:covered-table-cell>', cellStart);
          cellLen = 27;
        } else {
          cellEnd = rowContent.indexOf('</table:table-cell>', cellStart);
        }
        if (cellEnd === -1) break;

        const cellContent = rowContent.substring(cellStart, cellEnd + cellLen);
        const cellPlain = cellContent.replace(/<[^>]*>/g, ' ').trim();
        if (cellPlain) {
          if (cellCount > 0) cellText += ' | ';
          cellText += cellPlain;
          cellCount++;
        }
        cellPos = cellEnd + cellLen;
      }

      if (cellText && rowText) rowText += '\n';
      if (cellText) rowText += cellText;
      pos = rowEnd + 18;
    }

    const replacement = rowText
      ? '\n---TABLE---\n' + rowText + '\n---TABLE---\n'
      : '\n---TABLE---\n(Empty table)\n---TABLE---\n';
    text = text.substring(0, tableStart) + replacement + text.substring(tableEnd + 14);
  }

  // 3. Strip namespace prefixes only from non-table (XML) sections.
  {
    const marker = '---TABLE---';
    const parts = text.split(marker);
    for (let i = 0; i < parts.length; i += 2) {
      parts[i] = parts[i].replace(/([a-z]+):/g, '');
    }
    text = parts.join(marker);
  }

  // 4. Strip all remaining XML tags.
  text = text.replace(/<[^>]*>/g, ' ');

  // 5. Collapse runs of spaces (preserve newlines).
  text = text.replace(/ {2,}/g, ' ');

  return text.trim();
}
