// Bulk Checksum tab — compute MD5/SHA-1/CRC-32 for a list of file paths.

import { useState } from 'react';
import type { ChecksumResult, LogType } from '../types';
import type { Toast } from '../App';
import { api } from '../api';

interface Props {
  logMessage: (msg: string, type?: LogType) => void;
  showToast: (t: Toast) => void;
}

export function BulkChecksumTab({ logMessage, showToast }: Props) {
  const [text, setText] = useState('');
  const [results, setResults] = useState<ChecksumResult[]>([]);
  const [busy, setBusy] = useState(false);

  async function compute() {
    const paths = text
      .split(/\r?\n/)
      .map((l) => l.trim())
      .filter(Boolean);
    if (!paths.length) {
      showToast({ type: 'error', msg: 'Enter one or more file paths (one per line).' });
      return;
    }
    setBusy(true);
    logMessage(`Hashing ${paths.length} file(s)…`, 'info');
    try {
      const res = await api.checksum(paths);
      setResults(res);
      const ok = res.filter((r) => r.success).length;
      logMessage(`Hashed ${ok}/${res.length} file(s) successfully.`, ok === res.length ? 'success' : 'warning');
      showToast({ type: 'success', msg: `Hashed ${ok}/${res.length} files.` });
    } catch (e) {
      logMessage(`Bulk checksum failed: ${(e as Error).message}`, 'error');
      showToast({ type: 'error', msg: (e as Error).message });
    } finally {
      setBusy(false);
    }
  }

  function fmtSize(b?: number) {
    if (b == null) return '—';
    if (b < 1024) return `${b} B`;
    if (b < 1024 * 1024) return `${(b / 1024).toFixed(1)} KB`;
    return `${(b / 1024 / 1024).toFixed(1)} MB`;
  }

  return (
    <div className="col" style={{ gap: 18 }}>
      <section className="panel" style={{ padding: 18 }}>
        <div className="section-title">Bulk Checksum</div>
        <p className="muted" style={{ marginTop: 0 }}>
          Enter file paths (one per line). Local paths and UNC network shares (<span className="mono">\\server\share\file</span>)
          are supported — the server reads and hashes them.
        </p>
        <textarea
          value={text}
          onChange={(e) => setText(e.target.value)}
          placeholder={'E:\\builds\\app.exe\n\\\\fileserver\\release\\module.zip'}
          style={{
            width: '100%',
            minHeight: 130,
            background: 'rgba(2,6,23,0.6)',
            color: 'var(--text)',
            border: '1px solid var(--panel-border)',
            borderRadius: 8,
            padding: 12,
            fontFamily: 'Consolas, monospace',
            fontSize: 13,
            resize: 'vertical',
          }}
        />
        <div className="row" style={{ marginTop: 12 }}>
          <button className="btn-primary" onClick={compute} disabled={busy}>
            {busy ? (
              <span className="row" style={{ gap: 8 }}>
                <span className="spinner" /> Hashing…
              </span>
            ) : (
              '# Compute Checksums'
            )}
          </button>
          {results.length > 0 && (
            <button className="btn-ghost" onClick={() => setResults([])} disabled={busy}>
              Clear
            </button>
          )}
        </div>
      </section>

      {results.length > 0 && (
        <div className="panel" style={{ padding: 0, overflow: 'hidden' }}>
          <div style={{ maxHeight: 460, overflow: 'auto' }}>
            <table className="audit">
              <thead>
                <tr>
                  <th>File</th>
                  <th>Size</th>
                  <th>MD5</th>
                  <th>SHA-1</th>
                  <th>CRC-32</th>
                  <th>Status</th>
                </tr>
              </thead>
              <tbody>
                {results.map((r, i) => (
                  <tr key={i} className="main">
                    <td className="mono" style={{ fontSize: 12, wordBreak: 'break-all' }}>
                      {r.path}
                    </td>
                    <td>{fmtSize(r.sizeBytes)}</td>
                    <td className="mono">{r.success ? r.md5 : '—'}</td>
                    <td className="mono">{r.success ? r.sha1 : '—'}</td>
                    <td className="mono">{r.success ? r.crc32 : '—'}</td>
                    <td>
                      {r.success ? (
                        <span className="badge MATCH">OK</span>
                      ) : (
                        <span className="badge ERROR" title={r.error}>
                          ERROR
                        </span>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}
