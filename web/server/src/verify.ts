// Verification engine — ports resolveFilePathForRecord, stripVersionSuffix, the
// config<->VDD matching, and the MATCH/MISMATCH/MISSING/CONFIG_ONLY status rules
// from src/mainwindow.cpp.

import { existsSync, statSync, readdirSync } from 'node:fs';
import type { ConfigItem, VddRecord } from './types.js';
import { makeRecord } from './types.js';
import { hashFile, normalizePath } from './hash.js';

/** QFileInfo::baseName equivalent: filename without dir, truncated at first '.'. */
function qBaseName(p: string): string {
  const base = (p || '').replace(/\\/g, '/').split('/').pop() || '';
  const dot = base.indexOf('.');
  return dot === -1 ? base : base.slice(0, dot);
}

function lower(s: string): string {
  return (s || '').toLowerCase().trim();
}

/** Port of MainWindow::stripVersionSuffix. */
export function stripVersionSuffix(name: string): string {
  let res = (name || '').trim().toLowerCase();
  const exts = ['.zip', '.7z', '.exe', '.tar', '.tgz', '.gz', '.rar'];
  for (const e of exts) {
    if (res.endsWith(e)) {
      res = res.slice(0, -e.length);
      break;
    }
  }
  res = res.replace(/ /g, '');
  const rx1 = /-[0-9]+[vr][0-9a-z]+$/;
  const rx2 = /-[0-9]+\.[0-9]+$/;
  const rx3 = /-[vr][0-9]+$/;
  if (rx1.test(res)) res = res.replace(rx1, '');
  else if (rx2.test(res)) res = res.replace(rx2, '');
  else if (rx3.test(res)) res = res.replace(rx3, '');
  while (res.endsWith('-') || res.endsWith('_')) res = res.slice(0, -1);
  return res;
}

/** Strip the punctuation Qt strips when doing robust baseName matching. */
function stripPunct(s: string): string {
  return s.replace(/[.\-_]/g, '');
}

/** Recursively collect files under a directory (bounded depth) for fuzzy matching. */
function walkFiles(dir: string, maxDepth = 4): string[] {
  const out: string[] = [];
  const recurse = (d: string, depth: number) => {
    if (depth > maxDepth) return;
    let entries;
    try {
      entries = readdirSync(d, { withFileTypes: true });
    } catch {
      return;
    }
    for (const e of entries) {
      const full = d.replace(/[\\/]$/, '') + '/' + e.name;
      if (e.isDirectory()) recurse(full, depth + 1);
      else out.push(full);
    }
  };
  recurse(dir, 0);
  return out;
}

/** Port of MainWindow::resolveFilePathForRecord. Returns '' if no file found. */
export function resolveFilePathForRecord(rec: VddRecord, configItems: ConfigItem[]): string {
  const fnKey = lower(rec.fileName);
  const refKey = lower(rec.ciReference);
  const refBase = qBaseName(refKey);
  const fnBase = qBaseName(fnKey);

  const fileLookup = new Map<string, string>();
  const baseLookup = new Map<string, string>();
  for (const item of configItems) {
    const ciKey = lower(item.fileName);
    const ciBase = qBaseName(ciKey).toLowerCase();
    fileLookup.set(ciKey, item.documentLink || '');
    if (!baseLookup.has(ciBase)) baseLookup.set(ciBase, item.documentLink || '');
  }

  let docLink = '';
  if (fileLookup.has(refKey)) docLink = fileLookup.get(refKey)!;
  else if (baseLookup.has(refBase)) docLink = baseLookup.get(refBase)!;
  else if (fileLookup.has(fnKey)) docLink = fileLookup.get(fnKey)!;
  else if (baseLookup.has(fnBase)) docLink = baseLookup.get(fnBase)!;
  else {
    for (const [configKey, val] of fileLookup) {
      const configBase = qBaseName(configKey).toLowerCase();
      if (
        (refKey.length >= 12 && configKey.length >= 12 && (refKey.includes(configKey) || configKey.includes(refKey))) ||
        (refBase.length >= 12 && configBase.length >= 12 && (refBase.includes(configBase) || configBase.includes(refBase)))
      ) {
        docLink = val;
        break;
      }
    }
  }

  if (!docLink) return '';
  const docLinkClean = normalizePath(docLink.trim());
  if (!docLinkClean) return '';

  // Try 1: exact file path.
  try {
    const st = statSync(docLinkClean);
    if (st.isFile()) return docLinkClean;
  } catch {
    /* not a direct file */
  }

  // Try 2: treat as directory and search for the file.
  let isDir = false;
  try {
    isDir = statSync(docLinkClean).isDirectory();
  } catch {
    isDir = false;
  }
  if (!isDir) return '';

  const ciBase = qBaseName(rec.ciReference);
  const wantNames = [rec.fileName.toLowerCase(), ciBase.toLowerCase()].filter(Boolean);
  const ciBaseStripped = stripPunct(ciBase.toLowerCase());

  let entries: string[] = [];
  try {
    entries = readdirSync(docLinkClean);
  } catch {
    return '';
  }

  // Exact / case-insensitive / contains match at top level.
  for (const f of entries) {
    const lf = f.toLowerCase();
    const checkBase = stripPunct(qBaseName(f).toLowerCase());
    if (
      checkBase === ciBaseStripped ||
      wantNames.includes(lf) ||
      (ciBase && lf.includes(ciBase.toLowerCase()))
    ) {
      const full = docLinkClean.replace(/[\\/]$/, '') + '/' + f;
      try {
        if (statSync(full).isFile()) return full;
      } catch {
        /* skip */
      }
    }
  }

  // Recursive fallback.
  for (const full of walkFiles(docLinkClean)) {
    const name = full.replace(/[\\/]$/, '').split('/').pop() || '';
    const subBase = stripPunct(qBaseName(name).toLowerCase());
    if (subBase === ciBaseStripped || wantNames.includes(name.toLowerCase()) || (ciBase && name.toLowerCase().includes(ciBase.toLowerCase()))) {
      return full;
    }
  }

  return '';
}

function cleanHash(hash: string): string {
  let h = (hash || '').trim().toLowerCase();
  if (h.startsWith('0x')) h = h.slice(2);
  return h;
}

/** Try to match a config item to an existing VDD record; return its index or -1. */
function findMatchingVdd(item: ConfigItem, records: VddRecord[]): number {
  const ciKey = lower(item.fileName);
  const ciKeyClean = ciKey.replace(/ /g, '');
  const ciBase = qBaseName(ciKey).toLowerCase();
  const ciBaseClean = ciBase.replace(/ /g, '');

  for (let i = 0; i < records.length; i++) {
    const rec = records[i];
    if (rec.source === 'CONFIG_ONLY') continue;
    const recFn = lower(rec.fileName);
    const recFnClean = recFn.replace(/ /g, '');
    const recFnBase = qBaseName(recFn).toLowerCase();
    const recFnBaseClean = recFnBase.replace(/ /g, '');
    const recCiRef = lower(rec.ciReference);
    const recCiRefClean = recCiRef.replace(/ /g, '');

    if (ciKey === recFn) return i;
    if (ciKeyClean && ciKeyClean === recFnClean) return i;
    if (ciBase === recFnBase) return i;
    if (ciBaseClean && ciBaseClean === recFnBaseClean) return i;
    if (ciKey === recCiRef) return i;
    if (ciKeyClean && ciKeyClean === recCiRefClean) return i;
    if (ciBase === recCiRef) return i;
    if (ciBaseClean === recCiRefClean) return i;

    if (
      (ciKey.length >= 12 && recFn.length >= 12 && (ciKey.includes(recFn) || recFn.includes(ciKey))) ||
      (ciBase.length >= 12 && recFnBase.length >= 12 && (ciBase.includes(recFnBase) || recFnBase.includes(ciBase))) ||
      (ciKey.length >= 12 && recCiRef.length >= 12 && (ciKey.includes(recCiRef) || recCiRef.includes(ciKey))) ||
      (ciBase.length >= 12 && recCiRef.length >= 12 && (ciBase.includes(recCiRef) || recCiRef.includes(ciBase)))
    ) {
      return i;
    }
  }

  // Suffix-stripped fallback.
  const ciStripped = stripVersionSuffix(ciKey);
  for (let i = 0; i < records.length; i++) {
    if (records[i].source === 'CONFIG_ONLY') continue;
    if (ciStripped === stripVersionSuffix(records[i].fileName) || ciStripped === stripVersionSuffix(records[i].ciReference)) {
      return i;
    }
  }
  return -1;
}

export interface VerifyResult {
  records: VddRecord[];
  configItemCount: number;
}

/**
 * Run a full verification pass: merge config items (attaching metadata to matched
 * VDD records, creating CONFIG_ONLY records for the rest), resolve + hash each file,
 * and compute statuses.
 */
export async function verify(vddRecords: VddRecord[], configFilePaths: string[], configItems: ConfigItem[]): Promise<VerifyResult> {
  // Work on a copy.
  const records: VddRecord[] = vddRecords.map((r) => ({ ...r }));

  // Merge config items.
  let configOnlyCount = 0;
  const baseId = records.length;
  for (const item of configItems) {
    const idx = findMatchingVdd(item, records);
    if (idx !== -1) {
      const rec = records[idx];
      if (!rec.configFileName) {
        rec.configFileName = item.fileName.trim();
        rec.configVersion = item.version.trim();
        rec.configPath = item.documentLink.trim();
        rec.configComponent = item.component.trim();
      }
      continue;
    }
    configOnlyCount++;
    records.push(
      makeRecord({
        id: -(baseId + configOnlyCount),
        source: 'CONFIG_ONLY',
        fileName: item.fileName,
        ciReference: item.fileName,
        configFileName: item.fileName,
        configVersion: item.version,
        configPath: item.documentLink,
        configComponent: item.component,
        expectedMd5: item.expectedMd5,
        localCiRef: qBaseName(item.fileName),
        localStatus: 'CONFIG_ONLY',
      })
    );
  }

  // Resolve + hash + compare each record.
  for (const rec of records) {
    const fullPath = resolveFilePathForRecord(rec, configItems);

    if (!fullPath) {
      if (rec.source === 'CONFIG_ONLY') {
        rec.localStatus = 'CONFIG_ONLY';
        rec.localFileName = 'NOT FOUND';
        rec.localCiRef = 'N/A';
        rec.localStatusReason = 'CI item is in configuration but file not found at Document Link path.';
      } else {
        rec.localStatus = 'MISSING';
        rec.localFileName = 'NOT FOUND';
        rec.localCiRef = 'N/A';
        rec.localStatusReason = 'File not found in configuration Document Link paths.';
      }
      continue;
    }

    rec.localFileName = fullPath.replace(/[\\/]$/, '').split(/[\\/]/).pop() || '';
    rec.localCiRef = qBaseName(rec.localFileName);
    rec.localFullPath = fullPath;

    const h = await hashFile(fullPath);
    if (!h.success) {
      rec.localStatus = rec.source === 'CONFIG_ONLY' ? 'CONFIG_ONLY' : 'ERROR';
      rec.calculatedMd5 = h.error || 'Hash error';
      rec.localStatusReason = `Hashing failed for '${rec.fileName}'. ${h.error || ''}`.trim();
      continue;
    }

    rec.calculatedMd5 = h.md5;
    rec.calculatedSha1 = h.sha1;
    rec.calculatedCrc32 = h.crc32;

    if (rec.source === 'CONFIG_ONLY') {
      rec.localStatus = 'CONFIG_ONLY';
      rec.localStatusReason = `Physical file verified: calculated MD5 = ${h.md5} (config-only item, no VDD reference).`;
      continue;
    }

    const expMd5 = cleanHash(rec.expectedMd5);
    const expCrc = cleanHash(rec.expectedCrc32);
    const calcMd5 = cleanHash(h.md5);
    const calcCrc = cleanHash(h.crc32);
    const hasExpectedMd5 = !!expMd5;
    const hasExpectedCrc = !!expCrc;
    const md5Matches = !hasExpectedMd5 || expMd5 === calcMd5;
    const crcMatches = !hasExpectedCrc || expCrc === calcCrc;

    if (md5Matches && crcMatches) {
      rec.localStatus = 'MATCH';
      if (hasExpectedMd5 && hasExpectedCrc) rec.localStatusReason = 'Physical file verified: MD5 and CRC-32 match VDD expected checksums.';
      else if (hasExpectedMd5) rec.localStatusReason = 'Physical file verified: MD5 matches VDD expected checksum.';
      else if (hasExpectedCrc) rec.localStatusReason = 'Physical file verified: CRC-32 matches VDD expected checksum.';
      else rec.localStatusReason = 'Physical file verified (no integrity checksums specified in VDD).';
    } else {
      rec.localStatus = 'MISMATCH';
      const errors: string[] = [];
      if (!md5Matches) errors.push(`Calculated MD5 '${h.md5}' does not match VDD expected '${rec.expectedMd5}'`);
      if (!crcMatches) errors.push(`Calculated CRC-32 '${h.crc32}' does not match VDD expected '${rec.expectedCrc32}'`);
      rec.localStatusReason = 'Integrity violation: ' + errors.join(' / ');
    }
  }

  return { records, configItemCount: configItems.length };
}
