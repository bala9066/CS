// Audit results table with collapsible per-row hash detail. Mirrors the desktop
// table's columns and expand-to-show-detail behavior.

import { useState } from 'react';
import type { VddRecord } from '../types';

interface Props {
  records: VddRecord[];
  showCrc32: boolean;
}

export function AuditTable({ records, showCrc32 }: Props) {
  const [expanded, setExpanded] = useState<Set<number>>(new Set());

  const toggle = (id: number) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      next.has(id) ? next.delete(id) : next.add(id);
      return next;
    });
  };

  if (!records.length) {
    return (
      <div className="panel" style={{ padding: 40, textAlign: 'center' }}>
        <div style={{ fontSize: 40, marginBottom: 8 }}>▦</div>
        <h3 style={{ margin: '0 0 6px' }}>No records yet</h3>
        <p className="muted" style={{ margin: 0 }}>
          Import a VDD document to begin, then Verify against your configuration.
        </p>
      </div>
    );
  }

  return (
    <div className="panel" style={{ padding: 0, overflow: 'hidden' }}>
      <div style={{ maxHeight: 540, overflow: 'auto' }}>
        <table className="audit">
          <thead>
            <tr>
              <th style={{ width: 26 }}></th>
              <th>File / CI Reference</th>
              <th>Version</th>
              <th>Source</th>
              <th>Component</th>
              <th>Local File</th>
              <th>Expected MD5</th>
              {showCrc32 && <th>Expected CRC-32</th>}
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            {records.map((r) => {
              const isOpen = expanded.has(r.id);
              return (
                <RowGroup key={r.id} r={r} isOpen={isOpen} onToggle={() => toggle(r.id)} showCrc32={showCrc32} />
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function RowGroup({
  r,
  isOpen,
  onToggle,
  showCrc32,
}: {
  r: VddRecord;
  isOpen: boolean;
  onToggle: () => void;
  showCrc32: boolean;
}) {
  const colSpan = showCrc32 ? 9 : 8;
  return (
    <>
      <tr className="main" onClick={onToggle}>
        <td style={{ color: 'var(--indigo-bright)' }}>{isOpen ? '▾' : '▸'}</td>
        <td>
          <div style={{ fontWeight: 600 }}>{r.fileName || r.ciReference}</div>
          {r.ciReference && r.ciReference !== r.fileName && (
            <div className="muted mono" style={{ fontSize: 11 }}>
              {r.ciReference}
            </div>
          )}
        </td>
        <td>{r.version || r.configVersion || '—'}</td>
        <td>
          <span className="muted">{r.source}</span>
        </td>
        <td>{r.configComponent || '—'}</td>
        <td className="mono" style={{ fontSize: 12 }}>
          {r.localFileName || '—'}
        </td>
        <td className="mono">{r.expectedMd5 || '—'}</td>
        {showCrc32 && <td className="mono">{r.expectedCrc32 || '—'}</td>}
        <td>
          <span className={`badge ${r.localStatus}`}>{r.localStatus}</span>
        </td>
      </tr>
      {isOpen && (
        <tr className="detail-row">
          <td></td>
          <td colSpan={colSpan}>
            <div style={{ display: 'grid', gridTemplateColumns: '140px 1fr 1fr', gap: '6px 16px', padding: '6px 0' }}>
              <div className="muted">Hash</div>
              <div className="muted">Expected</div>
              <div className="muted">Calculated</div>

              <div>MD5</div>
              <div className="mono">{r.expectedMd5 || '—'}</div>
              <div className="mono">{hashCell(r.calculatedMd5, r.expectedMd5)}</div>

              <div>SHA-1</div>
              <div className="mono">—</div>
              <div className="mono">{r.calculatedSha1 || '—'}</div>

              <div>CRC-32</div>
              <div className="mono">{r.expectedCrc32 || '—'}</div>
              <div className="mono">{hashCell(r.calculatedCrc32, r.expectedCrc32)}</div>

              <div className="muted" style={{ marginTop: 8 }}>
                Path
              </div>
              <div className="mono" style={{ gridColumn: '2 / span 2', marginTop: 8, wordBreak: 'break-all' }}>
                {r.localFullPath || r.configPath || '—'}
              </div>

              <div className="muted" style={{ marginTop: 4 }}>
                Reason
              </div>
              <div style={{ gridColumn: '2 / span 2', marginTop: 4 }}>{r.localStatusReason || '—'}</div>
            </div>
          </td>
        </tr>
      )}
    </>
  );
}

function hashCell(calc: string, expected: string) {
  if (!calc) return '—';
  if (!expected) return calc;
  const match = calc.toLowerCase() === expected.toLowerCase();
  return (
    <span style={{ color: match ? 'var(--green)' : 'var(--red)' }}>
      {calc} {match ? '✓' : '✗'}
    </span>
  );
}
