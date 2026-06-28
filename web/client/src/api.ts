// Thin API client for the backend. All calls go through the Vite /api proxy.

import type { VddRecord, PublicSettings, ChecksumResult } from './types';

async function post<T>(url: string, body: unknown): Promise<T> {
  const resp = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  const data = await resp.json();
  if (!resp.ok || data.ok === false) {
    throw new Error(data.error || `Request failed (${resp.status})`);
  }
  return data as T;
}

async function get<T>(url: string): Promise<T> {
  const resp = await fetch(url);
  const data = await resp.json();
  if (!resp.ok || data.ok === false) throw new Error(data.error || `Request failed (${resp.status})`);
  return data as T;
}

export const api = {
  getSettings: () => get<{ settings: PublicSettings }>('/api/settings').then((d) => d.settings),

  saveSettings: (patch: Partial<PublicSettings> & { accessToken?: string }) =>
    post<{ settings: PublicSettings }>('/api/settings', patch).then((d) => d.settings),

  discoverModels: () => post<{ models: string[] }>('/api/llm/models', {}).then((d) => d.models),

  query: (prompt: string, systemPrompt = '') =>
    post<{ text: string }>('/api/llm/query', { prompt, systemPrompt }).then((d) => d.text),

  importVddByPath: (path: string) =>
    post<{ records: VddRecord[]; raw: string; extractedChars: number }>('/api/vdd/import', { path }),

  importVddUpload: async (file: File) => {
    const form = new FormData();
    form.append('file', file);
    const resp = await fetch('/api/vdd/import', { method: 'POST', body: form });
    const data = await resp.json();
    if (!resp.ok || data.ok === false) throw new Error(data.error || 'Import failed');
    return data as { records: VddRecord[]; raw: string; extractedChars: number };
  },

  extractOdtByPath: (path: string) =>
    post<{ text: string }>('/api/odt/extract', { path }).then((d) => d.text),

  extractOdtUpload: async (file: File) => {
    const form = new FormData();
    form.append('file', file);
    const resp = await fetch('/api/odt/extract', { method: 'POST', body: form });
    const data = await resp.json();
    if (!resp.ok || data.ok === false) throw new Error(data.error || 'Extraction failed');
    return (data as { text: string }).text;
  },

  parseConfig: (paths: string[]) =>
    post<{ items: unknown[] }>('/api/config/parse', { paths }).then((d) => d.items),

  verify: (records: VddRecord[], configPaths: string[]) =>
    post<{ records: VddRecord[]; configItemCount: number }>('/api/verify', { records, configPaths }),

  checksum: (paths: string[]) =>
    post<{ results: ChecksumResult[] }>('/api/checksum', { paths }).then((d) => d.results),
};
