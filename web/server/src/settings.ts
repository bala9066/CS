// Settings persistence — replaces QSettings (registry) from AIConfigManager.
// Stored in settings.json next to the server. Seeded from .env on first run.
// The access token is kept server-side and never sent to the browser.

import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import type { Settings, PublicSettings } from './types.js';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SETTINGS_PATH = join(__dirname, '..', 'settings.json');

const DEFAULTS: Settings = {
  gatewayUrl: process.env.GATEWAY_URL || 'https://api.anthropic.com/v1',
  accessToken: process.env.ACCESS_TOKEN || '',
  temperature: process.env.TEMPERATURE ? Number(process.env.TEMPERATURE) : 0,
  maxTokens: process.env.MAX_TOKENS ? Number(process.env.MAX_TOKENS) : 4096,
  targetModel: process.env.TARGET_MODEL || 'gemini-2.0-flash',
};

let cache: Settings | null = null;

export function getSettings(): Settings {
  if (cache) return cache;
  if (existsSync(SETTINGS_PATH)) {
    try {
      const raw = JSON.parse(readFileSync(SETTINGS_PATH, 'utf8'));
      cache = { ...DEFAULTS, ...raw };
      return cache!;
    } catch {
      // fall through to defaults
    }
  }
  cache = { ...DEFAULTS };
  return cache;
}

export function updateSettings(patch: Partial<Settings>): Settings {
  const current = getSettings();
  // Empty-string accessToken in a patch means "leave unchanged" so the UI never
  // has to round-trip the secret.
  const next: Settings = { ...current, ...patch };
  if (patch.accessToken === '' || patch.accessToken === undefined) {
    next.accessToken = current.accessToken;
  }
  cache = next;
  writeFileSync(SETTINGS_PATH, JSON.stringify(next, null, 2), 'utf8');
  return next;
}

export function toPublic(s: Settings): PublicSettings {
  const { accessToken, ...rest } = s;
  return { ...rest, hasAccessToken: !!accessToken };
}
