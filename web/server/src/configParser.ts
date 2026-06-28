// Config file parsing — ports parseConfigFileItems / parseCSV / parseCSVLine and the
// XLSX path from src/mainwindow.cpp. Produces CI items consumed by verification.

import { readFileSync, existsSync } from 'node:fs';
import { extname } from 'node:path';
import * as XLSX from 'xlsx';
import type { ConfigItem } from './types.js';

/** Parse one delimited line honoring RFC-4180 quoting. */
export function parseCSVLine(line: string, delimiter = ','): string[] {
  const result: string[] = [];
  let cur = '';
  let inQuotes = false;
  for (let i = 0; i < line.length; i++) {
    const ch = line[i];
    if (ch === '"') {
      if (inQuotes && line[i + 1] === '"') {
        cur += '"';
        i++;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (ch === delimiter && !inQuotes) {
      result.push(cur);
      cur = '';
    } else {
      cur += ch;
    }
  }
  result.push(cur);
  return result;
}

function stripQuotes(v: string): string {
  v = v.trim();
  if (v.length >= 2 && v.startsWith('"') && v.endsWith('"')) v = v.slice(1, -1).trim();
  return v;
}

/** Read a config file (CSV or XLSX) into raw rows (each row is the raw line/joined cells). */
function readRows(filePath: string): { rows: string[]; delimiter: string } {
  const ext = extname(filePath).toLowerCase();
  if (ext === '.csv') {
    const content = readFileSync(filePath, 'utf8');
    const rows = content.split(/\r\n|\n|\r/).filter((l, i, arr) => !(i === arr.length - 1 && l === ''));
    let delimiter = ',';
    if (rows.length) {
      const first = rows[0];
      if ((first.match(/;/g) || []).length > (first.match(/,/g) || []).length) delimiter = ';';
    }
    return { rows, delimiter };
  }
  if (ext === '.xlsx' || ext === '.xls' || ext === '.xlsm') {
    const wb = XLSX.readFile(filePath);
    const sheet = wb.Sheets[wb.SheetNames[0]];
    const aoa: string[][] = XLSX.utils.sheet_to_json(sheet, { header: 1, raw: false, defval: '' });
    // Join each row back with a delimiter we won't clash with, then reuse CSV path.
    const rows = aoa.map((r) => r.map((c) => String(c ?? '')).join(''));
    return { rows, delimiter: '' };
  }
  return { rows: [], delimiter: ',' };
}

/** Parse one or more config files into a deduped list of CI items. */
export function parseConfigFileItems(filePaths: string[]): ConfigItem[] {
  const parsedItems: ConfigItem[] = [];

  for (const filePath of filePaths) {
    if (!existsSync(filePath)) continue;
    const { rows, delimiter } = readRows(filePath);
    if (!rows.length) continue;

    // Headers (strip HTML tags from first row first).
    const cleanFirstLine = rows[0].replace(/<[^>]+>/g, ' ');
    const headers = parseCSVLine(cleanFirstLine, delimiter).map((h) => h.trim());

    let ciRefCol = -1, ciVerCol = -1, md5Col = -1, docLinkCol = -1, compCol = -1;
    for (let i = 0; i < headers.length; i++) {
      const head = headers[i].toLowerCase().replace(/_/g, ' ').replace(/-/g, ' ');
      if (head.includes('ci reference') || head === 'ci ref' || (head.includes('ci') && head.includes('ref'))) {
        if (ciRefCol === -1) ciRefCol = i;
      } else if (head === 'version' || head.includes('ci version')) {
        if (ciVerCol === -1) ciVerCol = i;
      } else if (head.includes('md5') || head.includes('checksum')) {
        if (md5Col === -1) md5Col = i;
      } else if (head.includes('document link') || head.includes('doc link') || head.includes('link')) {
        if (docLinkCol === -1) docLinkCol = i;
      } else if (head.includes('crq') || head.includes('component')) {
        if (compCol === -1) compCol = i;
      }
    }

    // Fallback CI-reference column detection.
    if (ciRefCol === -1) {
      if (rows.length > 1) {
        const firstData = parseCSVLine(rows[1], delimiter);
        for (let i = 0; i < firstData.length; i++) {
          const cell = firstData[i].trim();
          if (cell.includes('-') && cell.length >= 5) {
            ciRefCol = i;
            break;
          }
        }
      }
      if (ciRefCol === -1 && headers.length > 5) ciRefCol = 5;
    }

    const seenKeys = new Set<string>();

    for (let r = 1; r < rows.length; r++) {
      if (!rows[r].trim()) continue;
      const cells = parseCSVLine(rows[r], delimiter);
      if (ciRefCol < 0 || ciRefCol >= cells.length) continue;

      let ciRef = stripQuotes(cells[ciRefCol]).replace(/<[^>]+>/g, '').trim();
      if (!ciRef) continue;

      const normKey = ciRef.toLowerCase().replace(/ /g, '');
      if (seenKeys.has(normKey)) continue;
      seenKeys.add(normKey);

      const get = (col: number) => (col !== -1 && col < cells.length ? stripQuotes(cells[col]) : '');

      parsedItems.push({
        fileName: ciRef,
        version: get(ciVerCol),
        expectedMd5: get(md5Col),
        documentLink: get(docLinkCol),
        component: get(compCol),
      });
    }
  }

  return parsedItems;
}
