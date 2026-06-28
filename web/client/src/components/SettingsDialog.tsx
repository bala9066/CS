// Settings dialog — LLM gateway URL, access token (user-entered), temperature,
// max tokens, model selector with Discover Models. Mirrors the desktop SettingsDialog.

import { useState } from 'react';
import type { PublicSettings } from '../types';
import type { Toast } from '../App';
import { api } from '../api';

interface Props {
  current: PublicSettings | null;
  onClose: () => void;
  onSaved: (s: PublicSettings) => void;
  showToast: (t: Toast) => void;
}

export function SettingsDialog({ current, onClose, onSaved, showToast }: Props) {
  const [gatewayUrl, setGatewayUrl] = useState(current?.gatewayUrl ?? '');
  const [accessToken, setAccessToken] = useState(''); // never prefilled
  const [temperature, setTemperature] = useState(current?.temperature ?? 0);
  const [maxTokens, setMaxTokens] = useState(current?.maxTokens ?? 4096);
  const [targetModel, setTargetModel] = useState(current?.targetModel ?? '');
  const [models, setModels] = useState<string[]>(current?.targetModel ? [current.targetModel] : []);
  const [busy, setBusy] = useState(false);

  async function discover() {
    setBusy(true);
    try {
      // Persist the URL/token first so discovery uses them.
      await api.saveSettings({ gatewayUrl, accessToken, temperature, maxTokens, targetModel });
      const found = await api.discoverModels();
      setModels(found);
      if (found.length && !found.includes(targetModel)) setTargetModel(found[0]);
      showToast({ type: 'success', msg: `Found ${found.length} model(s).` });
    } catch (e) {
      showToast({ type: 'error', msg: `Discovery failed: ${(e as Error).message}` });
    } finally {
      setBusy(false);
    }
  }

  async function save() {
    setBusy(true);
    try {
      const saved = await api.saveSettings({ gatewayUrl, accessToken, temperature, maxTokens, targetModel });
      onSaved(saved);
    } catch (e) {
      showToast({ type: 'error', msg: `Save failed: ${(e as Error).message}` });
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="modal-backdrop" onClick={onClose}>
      <div className="modal panel" onClick={(e) => e.stopPropagation()}>
        <h2>AI Gateway Settings</h2>

        <div className="field">
          <label>Gateway URL</label>
          <input
            value={gatewayUrl}
            onChange={(e) => setGatewayUrl(e.target.value)}
            placeholder="https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent"
          />
          <span className="muted" style={{ fontSize: 11 }}>
            Gemini, Anthropic (api.anthropic.com), or any OpenAI-compatible endpoint.
          </span>
        </div>

        <div className="field">
          <label>API Access Token {current?.hasAccessToken && <span className="muted">(stored — leave blank to keep)</span>}</label>
          <input
            type="password"
            value={accessToken}
            onChange={(e) => setAccessToken(e.target.value)}
            placeholder={current?.hasAccessToken ? '•••••••••• (unchanged)' : 'Paste your API token'}
            autoComplete="off"
          />
        </div>

        <div className="row" style={{ gap: 12 }}>
          <div className="field grow">
            <label>Temperature: {temperature.toFixed(2)}</label>
            <input
              type="range"
              min={0}
              max={1}
              step={0.05}
              value={temperature}
              onChange={(e) => setTemperature(Number(e.target.value))}
            />
          </div>
          <div className="field" style={{ width: 140 }}>
            <label>Max Tokens</label>
            <input type="number" value={maxTokens} onChange={(e) => setMaxTokens(Number(e.target.value))} />
          </div>
        </div>

        <div className="field">
          <label>Model</label>
          <div className="row">
            <select className="grow" value={targetModel} onChange={(e) => setTargetModel(e.target.value)}>
              {models.length === 0 && <option value="">(discover or type below)</option>}
              {models.map((m) => (
                <option key={m} value={m}>
                  {m}
                </option>
              ))}
            </select>
            <button className="btn-ghost" onClick={discover} disabled={busy}>
              {busy ? <span className="spinner" /> : 'Discover Models'}
            </button>
          </div>
          <input
            style={{ marginTop: 8 }}
            value={targetModel}
            onChange={(e) => setTargetModel(e.target.value)}
            placeholder="…or type a model id (e.g. gemini-2.0-flash)"
          />
        </div>

        <div className="row" style={{ justifyContent: 'flex-end', marginTop: 8 }}>
          <button className="btn-ghost" onClick={onClose} disabled={busy}>
            Cancel
          </button>
          <button className="btn-primary" onClick={save} disabled={busy}>
            Save Settings
          </button>
        </div>
      </div>
    </div>
  );
}
