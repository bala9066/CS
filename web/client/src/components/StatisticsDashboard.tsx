// Real-time stat cards + filter + CRC32 toggle. Mirrors StatisticsWidget.

import type { VddRecord } from '../types';

export type StatusFilter = 'All' | 'MATCH' | 'MISMATCH' | 'MISSING' | 'PENDING' | 'CONFIG_ONLY';

interface Props {
  records: VddRecord[];
  filter: StatusFilter;
  onFilter: (f: StatusFilter) => void;
  showCrc32: boolean;
  onToggleCrc32: (v: boolean) => void;
}

export function StatisticsDashboard({ records, filter, onFilter, showCrc32, onToggleCrc32 }: Props) {
  const count = (s: string) => records.filter((r) => r.localStatus === s).length;
  const total = records.length;
  const match = count('MATCH');
  const mismatch = count('MISMATCH');
  const missing = count('MISSING');
  const pending = count('PENDING');
  const configOnly = count('CONFIG_ONLY');
  const verified = match + mismatch;
  const successRate = verified > 0 ? Math.round((match / verified) * 100) : 0;

  const cards: Array<{ label: string; value: number; color: string; key: StatusFilter }> = [
    { label: 'Total', value: total, color: 'var(--indigo-bright)', key: 'All' },
    { label: 'Match', value: match, color: 'var(--green)', key: 'MATCH' },
    { label: 'Mismatch', value: mismatch, color: 'var(--red)', key: 'MISMATCH' },
    { label: 'Missing', value: missing, color: 'var(--amber)', key: 'MISSING' },
    { label: 'Config', value: configOnly, color: 'var(--purple)', key: 'CONFIG_ONLY' },
    { label: 'Pending', value: pending, color: 'var(--slate)', key: 'PENDING' },
  ];

  return (
    <div className="col" style={{ gap: 14 }}>
      <div className="stats">
        {cards.map((c) => (
          <button
            key={c.label}
            className="stat-card"
            onClick={() => onFilter(c.key)}
            style={{
              textAlign: 'left',
              outline: filter === c.key ? '2px solid var(--indigo)' : 'none',
            }}
          >
            <div className="stat-label" style={{ color: c.color }}>
              {c.label}
            </div>
            <div className="stat-value" style={{ color: c.color }}>
              {c.value}
            </div>
          </button>
        ))}
      </div>

      <div className="row">
        <div className="panel" style={{ padding: '10px 16px' }}>
          Success rate: <b style={{ color: 'var(--green)' }}>{successRate}%</b>{' '}
          <span className="muted">({match}/{verified || 0} verified)</span>
        </div>
        <div className="grow" />
        <label className="row" style={{ gap: 8, cursor: 'pointer' }}>
          <input type="checkbox" checked={showCrc32} onChange={(e) => onToggleCrc32(e.target.checked)} />
          <span>Show CRC-32</span>
        </label>
      </div>
    </div>
  );
}
