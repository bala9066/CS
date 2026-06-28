// Document Review tab — extract ODT text and run an LLM quality/CI review.

import { useRef, useState } from 'react';
import type { LogType } from '../types';
import type { Toast } from '../App';
import { api } from '../api';

interface Props {
  logMessage: (msg: string, type?: LogType) => void;
  showToast: (t: Toast) => void;
}

const REVIEW_SYSTEM_PROMPT =
  'You are a meticulous QA reviewer for Version Description Documents (VDDs). You will receive the extracted text of an ODT VDD. ' +
  'Assess document quality and configuration-item consistency. Report: (1) overall quality summary, (2) any CI items with missing/malformed CI references, versions, or checksums (MD5 must be 32 hex chars, CRC-32 must be 8 hex chars), ' +
  '(3) inconsistencies between file names and CI references, and (4) concrete fixes. Use clear head: sections and bullet points.';

export function DocumentReviewTab({ logMessage, showToast }: Props) {
  const [odtPath, setOdtPath] = useState('');
  const [extracted, setExtracted] = useState('');
  const [review, setReview] = useState('');
  const [busy, setBusy] = useState<string | null>(null);
  const [dragging, setDragging] = useState(false);
  const fileRef = useRef<HTMLInputElement>(null);

  async function extractPath() {
    if (!odtPath.trim()) {
      showToast({ type: 'error', msg: 'Enter an .odt path or drag a file in.' });
      return;
    }
    setBusy('extract');
    try {
      const text = await api.extractOdtByPath(odtPath.trim());
      setExtracted(text);
      logMessage(`Extracted ${text.length} chars from ${odtPath}.`, 'success');
    } catch (e) {
      logMessage(`Extraction failed: ${(e as Error).message}`, 'error');
      showToast({ type: 'error', msg: (e as Error).message });
    } finally {
      setBusy(null);
    }
  }

  async function extractFile(file: File) {
    setBusy('extract');
    setOdtPath(file.name);
    try {
      const text = await api.extractOdtUpload(file);
      setExtracted(text);
      logMessage(`Extracted ${text.length} chars from ${file.name}.`, 'success');
    } catch (e) {
      logMessage(`Extraction failed: ${(e as Error).message}`, 'error');
      showToast({ type: 'error', msg: (e as Error).message });
    } finally {
      setBusy(null);
    }
  }

  async function runReview() {
    if (!extracted) {
      showToast({ type: 'error', msg: 'Extract an ODT first.' });
      return;
    }
    setBusy('review');
    logMessage('Running LLM document review…', 'info');
    try {
      const text = await api.query(extracted, REVIEW_SYSTEM_PROMPT);
      setReview(text);
      logMessage('Document review complete.', 'success');
    } catch (e) {
      logMessage(`Review failed: ${(e as Error).message}`, 'error');
      showToast({ type: 'error', msg: (e as Error).message });
    } finally {
      setBusy(null);
    }
  }

  return (
    <div className="col" style={{ gap: 18 }}>
      <section className="panel" style={{ padding: 18 }}>
        <div className="section-title">Document Review</div>
        <div className="row">
          <input
            className="grow"
            placeholder="E:\path\to\document.odt"
            value={odtPath}
            onChange={(e) => setOdtPath(e.target.value)}
          />
          <button className="btn-primary" onClick={extractPath} disabled={!!busy}>
            {busy === 'extract' ? <Spin /> : 'Extract Text'}
          </button>
          <button className="btn-accent" onClick={runReview} disabled={!!busy || !extracted}>
            {busy === 'review' ? <Spin /> : '🔍 Run LLM Review'}
          </button>
        </div>
        <div
          className={`dropzone ${dragging ? 'drag' : ''}`}
          style={{ marginTop: 12 }}
          onDragOver={(e) => {
            e.preventDefault();
            setDragging(true);
          }}
          onDragLeave={() => setDragging(false)}
          onDrop={(e) => {
            e.preventDefault();
            setDragging(false);
            const f = e.dataTransfer.files?.[0];
            if (f) extractFile(f);
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
              if (f) extractFile(f);
            }}
          />
          Drag &amp; drop an <b>.odt</b> here, or click to browse.
        </div>
      </section>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
        <section className="panel" style={{ padding: 16 }}>
          <div className="section-title">Extracted Text</div>
          <pre
            style={{
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-word',
              maxHeight: 460,
              overflow: 'auto',
              margin: 0,
              fontSize: 12,
              color: 'var(--text-dim)',
            }}
          >
            {extracted || 'No text extracted yet.'}
          </pre>
        </section>
        <section className="panel" style={{ padding: 16 }}>
          <div className="section-title">LLM Review</div>
          <pre
            style={{
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-word',
              maxHeight: 460,
              overflow: 'auto',
              margin: 0,
              fontSize: 13,
            }}
          >
            {review || 'Run a review to see the assessment here.'}
          </pre>
        </section>
      </div>
    </div>
  );
}

function Spin() {
  return (
    <span className="row" style={{ gap: 8 }}>
      <span className="spinner" /> Working…
    </span>
  );
}
