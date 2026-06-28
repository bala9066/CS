import { useCallback, useEffect, useState } from 'react';
import type { LogEntry, LogType, PublicSettings } from './types';
import { api } from './api';
import { Landing } from './components/Landing';
import { HomeTab } from './components/HomeTab';
import { BulkChecksumTab } from './components/BulkChecksumTab';
import { DocumentReviewTab } from './components/DocumentReviewTab';
import { SettingsDialog } from './components/SettingsDialog';

type Tab = 'home' | 'bulk' | 'review';

export interface Toast {
  type: 'info' | 'success' | 'error';
  msg: string;
}

export default function App() {
  const [entered, setEntered] = useState(false);
  const [tab, setTab] = useState<Tab>('home');
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [settings, setSettings] = useState<PublicSettings | null>(null);
  const [toast, setToast] = useState<Toast | null>(null);
  const [log, setLog] = useState<LogEntry[]>([]);

  const showToast = useCallback((t: Toast) => {
    setToast(t);
    window.setTimeout(() => setToast(null), 4000);
  }, []);

  const logMessage = useCallback((msg: string, type: LogType = 'info') => {
    const time = new Date().toLocaleTimeString();
    setLog((prev) => [...prev, { time, type, msg }].slice(-300));
  }, []);

  const refreshSettings = useCallback(async () => {
    try {
      const s = await api.getSettings();
      setSettings(s);
    } catch (e) {
      logMessage(`Could not load settings: ${(e as Error).message}`, 'error');
    }
  }, [logMessage]);

  useEffect(() => {
    refreshSettings();
  }, [refreshSettings]);

  if (!entered) {
    return <Landing onEnter={() => setEntered(true)} />;
  }

  return (
    <>
      <header className="app-header">
        <div className="app-title">
          VDD <span className="accent">Audit</span> &amp; Verification
        </div>
        <nav className="tabs">
          <button className={`tab ${tab === 'home' ? 'active' : ''}`} onClick={() => setTab('home')}>
            Home
          </button>
          <button className={`tab ${tab === 'bulk' ? 'active' : ''}`} onClick={() => setTab('bulk')}>
            Bulk Checksum
          </button>
          <button className={`tab ${tab === 'review' ? 'active' : ''}`} onClick={() => setTab('review')}>
            Document Review
          </button>
        </nav>
        <button className="btn-ghost" style={{ marginLeft: 10 }} onClick={() => setSettingsOpen(true)}>
          ⚙ Settings
        </button>
      </header>

      <main className="content">
        {!settings?.hasAccessToken && (
          <div
            className="panel"
            style={{ padding: '12px 16px', marginBottom: 16, borderColor: 'rgba(245,158,11,0.4)' }}
          >
            <span className="muted">
              ⚠ No LLM access token configured. Open <b>Settings</b> and enter your gateway URL + API token to enable
              VDD extraction.
            </span>
          </div>
        )}

        {tab === 'home' && <HomeTab logMessage={logMessage} showToast={showToast} log={log} />}
        {tab === 'bulk' && <BulkChecksumTab logMessage={logMessage} showToast={showToast} />}
        {tab === 'review' && <DocumentReviewTab logMessage={logMessage} showToast={showToast} />}
      </main>

      {settingsOpen && (
        <SettingsDialog
          current={settings}
          onClose={() => setSettingsOpen(false)}
          onSaved={(s) => {
            setSettings(s);
            setSettingsOpen(false);
            showToast({ type: 'success', msg: 'Settings saved.' });
          }}
          showToast={showToast}
        />
      )}

      {toast && <div className={`toast ${toast.type}`}>{toast.msg}</div>}
    </>
  );
}
