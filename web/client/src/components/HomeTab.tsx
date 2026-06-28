// Home tab — the core audit workflow: config path + import VDD -> verify -> export.

import { useMemo, useRef, useState } from 'react';
import type { LogEntry, LogType, VddRecord } from '../types';
import type { Toast } from '../App';
import { api } from '../api';
import { StatisticsDashboard, type StatusFilter } from './StatisticsDashboard';
import { AuditTable } from './AuditTable';
import { ActionLog } from './ActionLog';

interface Props {
  logMessage: (msg: string, type?: LogType) => void;
  showToast: (t: Toast) => void;
  log: LogEntry[];
}

export function HomeTab({ logMessage, showToast, log }: Props) {
  const [configPath, setConfigPath] = useState('');
  const [odtPath, setOdtPath] = useState('');
  const [records, setRecords] = useState<VddRecord[]>([]);
  const [busy, setBusy] = useState<string | null>(null);
  const [filter, setFilter] = useState<StatusFilter>('All');
  const [showCrc32, setShowCrc32] = useState(false);
  const [dragging, setDragging] = useState(false);
  const fileRef = useRef<HTMLInputElement>(null);

  const filtered = useMemo(
    () => (filter === 'All' ? records : records.filter((r) => r.localStatus === filter)),
    [records, filter]
  );

  async function doImportPath() {
    if (!odtPath.trim()) {
      showToast({ type: 'error', msg: 'Enter the VDD .odt path or drag a file in.' });
      return;
    }
    setBusy('Importing VDD…');
    logMessage(`Importing VDD: ${odtPath}`, 'info');
    try {
      const res = await api.importVddByPath(odtPath.trim());
      applyImport(res);
    } catch (e) {
      logMessage(`AI extraction failed: ${(e as Error).message}`, 'error');
      showToast({ type: 'error', msg: `Import failed: ${(e as Error).message}` });
    } finally {
      setBusy(null);
    }
  }

  async function doImportFile(file: File) {
    setBusy('Importing VDD…');
    setOdtPath(file.name);
    logMessage(`Importing VDD (upload): ${file.name}`, 'info');
    try {
      const res = await api.importVddUpload(file);
      applyImport(res);
    } catch (e) {
      logMessage(`AI extraction failed: ${(e as Error).message}`, 'error');
      showToast({ type: 'error', msg: `Import failed: ${(e as Error).message}` });
    } finally {
      setBusy(null);
    }
  }

  function applyImport(res: { records: VddRecord[]; extractedChars: number }) {
    if (!res.records.length) {
      logMessage('LLM returned no CI items.', 'warning');
      showToast({ type: 'error', msg: 'No CI items extracted from the VDD.' });
      return;
    }
    setRecords(res.records);
    logMessage(`AI extracted ${res.records.length} CI item(s) from VDD (${res.extractedChars} chars).`, 'success');
    showToast({ type: 'success', msg: `Extracted ${res.records.length} CI items.` });
  }

  async function doVerify() {
    if (!records.length) {
      showToast({ type: 'error', msg: 'Import a VDD first.' });
      return;
    }
    if (!configPath.trim()) {
      showToast({ type: 'error', msg: 'Set the configuration CSV/XLSX path to resolve Document Links.' });
      return;
    }
    setBusy('Verifying…');
    logMessage('Resolving file paths and computing checksums…', 'info');
    try {
      const res = await api.verify(records, [configPath.trim()]);
      setRecords(res.records);
      const match = res.records.filter((r) => r.localStatus === 'MATCH').length;
      const mismatch = res.records.filter((r) => r.localStatus === 'MISMATCH').length;
      const missing = res.records.filter((r) => r.localStatus === 'MISSING').length;
      logMessage(
        `Verification complete: ${match} match, ${mismatch} mismatch, ${missing} missing (config items: ${res.configItemCount}).`,
        mismatch || missing ? 'warning' : 'success'
      );
      showToast({ type: 'success', msg: `Verified: ${match} match · ${mismatch} mismatch · ${missing} missing.` });
    } catch (e) {
      logMessage(`Verification failed: ${(e as Error).message}`, 'error');
      showToast({ type: 'error', msg: `Verify failed: ${(e as Error).message}` });
    } finally {
      setBusy(null);
    }
  }

  function exportCsv() {
    if (!records.length) return;
    const headers = [
      'File Name',
      'CI Reference',
      'Version',
      'Source',
      'Component',
      'Expected MD5',
      'Expected CRC32',
      'Calculated MD5',
      'Calculated SHA1',
      'Calculated CRC32',
      'Local File',
      'Local Path',
      'Status',
      'Reason',
    ];
    const esc = (v: string) => `"${(v ?? '').replace(/"/g, '""')}"`;
    const lines = [headers.join(',')];
    for (const r of records) {
      lines.push(
        [
          r.fileName,
          r.ciReference,
          r.version,
          r.source,
          r.configComponent,
          r.expectedMd5,
          r.expectedCrc32,
          r.calculatedMd5,
          r.calculatedSha1,
          r.calculatedCrc32,
          r.localFileName,
          r.localFullPath,
          r.localStatus,
          r.localStatusReason,
        ]
          .map((v) => esc(String(v ?? '')))
          .join(',')
      );
    }
    const blob = new Blob([lines.join('\r\n')], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `vdd-audit-${new Date().toISOString().slice(0, 10)}.csv`;
    a.click();
    URL.revokeObjectURL(url);
    logMessage(`Exported ${records.length} records to CSV.`, 'success');
  }

  return (
    <div className="col" style={{ gap: 18 }}>
      <section className="panel" style={{ padding: 18 }}>
        <div className="section-title">Audit Files</div>
        <div className="col" style={{ gap: 12 }}>
          <div className="row">
            <label style={{ width: 220 }} className="muted">
              Configuration Items (CSV/XLSX):
            </label>
            <input
              className="grow"
              placeholder="E:\path\to\config.csv  (path the server can read; supports UNC \\server\share)"
              value={configPath}
              onChange={(e) => setConfigPath(e.target.value)}
            />
          </div>

          <div className="row">
            <label style={{ width: 220 }} className="muted">
              VDD Document (.odt):
            </label>
            <input
              className="grow"
              placeholder="E:\path\to\document.odt  (or drag a file onto the box below)"
              value={odtPath}
              onChange={(e) => setOdtPath(e.target.value)}
            />
          </div>

          <div
            className={`dropzone ${dragging ? 'drag' : ''}`}
            onDragOver={(e) => {
              e.preventDefault();
              setDragging(true);
            }}
            onDragLeave={() => setDragging(false)}
            onDrop={(e) => {
              e.preventDefault();
              setDragging(false);
              const f = e.dataTransfer.files?.[0];
              if (f) doImportFile(f);
            }}
            onClick={() => fileRef.current?.click()}
          >
            <input
              ref={fileRef}
              type="file"
              accept=".odt"
              style={{ display: 'none' }}
              onChange={(e) => {
                const f = e.target.files?.[0];
                if (f) doImportFile(f);
              }}
            />
            Drag &amp; drop a <b>.odt</b> VDD here, or click to browse (uploads file bytes to the server).
          </div>

          <div className="row" style={{ marginTop: 4 }}>
            <button className="btn-primary" onClick={doImportPath} disabled={!!busy}>
              {busy === 'Importing VDD…' ? <Spinner label="Importing…" /> : '⬇ Import / Analyze VDD'}
            </button>
            <button className="btn-accent" onClick={doVerify} disabled={!!busy || !records.length}>
              {busy === 'Verifying…' ? <Spinner label="Verifying…" /> : '✔ Verify All'}
            </button>
            <button className="btn-green" onClick={exportCsv} disabled={!records.length}>
              ⤓ Export Results
            </button>
          </div>
        </div>
      </section>

      <StatisticsDashboard
        records={records}
        filter={filter}
        onFilter={setFilter}
        showCrc32={showCrc32}
        onToggleCrc32={setShowCrc32}
      />

      <AuditTable records={filtered} showCrc32={showCrc32} />

      <section>
        <div className="section-title">System Verification Log</div>
        <ActionLog log={log} />
      </section>
    </div>
  );
}

function Spinner({ label }: { label: string }) {
  return (
    <span className="row" style={{ gap: 8 }}>
      <span className="spinner" /> {label}
    </span>
  );
}
