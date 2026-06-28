// Express server for the VDD Audit web app.
// Routes mirror the desktop app's capabilities: settings, LLM proxy, ODT extraction,
// config parsing, verification, and bulk checksums. The LLM token stays server-side.

import express from 'express';
import cors from 'cors';
import multer from 'multer';
import { readFileSync } from 'node:fs';
import { getSettings, updateSettings, toPublic } from './settings.js';
import { queryText, discoverModels } from './llm.js';
import { extractPlainText, isValidOdt } from './odtExtractor.js';
import { parseConfigFileItems } from './configParser.js';
import { verify } from './verify.js';
import { hashFile } from './hash.js';
import { parseLLMJsonResponse, VDD_SYSTEM_PROMPT } from './vddParse.js';
import type { VddRecord } from './types.js';

const app = express();
app.use(cors());
app.use(express.json({ limit: '50mb' }));
const upload = multer({ storage: multer.memoryStorage(), limits: { fileSize: 100 * 1024 * 1024 } });

const PORT = Number(process.env.PORT) || 3001;

function fail(res: express.Response, status: number, error: string) {
  res.status(status).json({ ok: false, error });
}

// --- Health ---
app.get('/api/health', (_req, res) => res.json({ ok: true, name: 'vdd-audit-server' }));

// --- Settings ---
app.get('/api/settings', (_req, res) => {
  res.json({ ok: true, settings: toPublic(getSettings()) });
});

app.post('/api/settings', (req, res) => {
  const { gatewayUrl, accessToken, temperature, maxTokens, targetModel } = req.body ?? {};
  const patch: Record<string, unknown> = {};
  if (typeof gatewayUrl === 'string') patch.gatewayUrl = gatewayUrl;
  if (typeof accessToken === 'string') patch.accessToken = accessToken;
  if (typeof temperature === 'number') patch.temperature = temperature;
  if (typeof maxTokens === 'number') patch.maxTokens = maxTokens;
  if (typeof targetModel === 'string') patch.targetModel = targetModel;
  const next = updateSettings(patch);
  res.json({ ok: true, settings: toPublic(next) });
});

// --- LLM model discovery ---
app.post('/api/llm/models', async (_req, res) => {
  const result = await discoverModels(getSettings());
  if (!result.ok) return fail(res, 502, result.error || 'Discovery failed');
  res.json({ ok: true, models: result.models });
});

// --- Generic LLM query (used by Document Review) ---
app.post('/api/llm/query', async (req, res) => {
  const { prompt, systemPrompt } = req.body ?? {};
  if (typeof prompt !== 'string' || !prompt) return fail(res, 400, 'prompt is required');
  const result = await queryText(getSettings(), prompt, typeof systemPrompt === 'string' ? systemPrompt : '');
  if (!result.ok) return fail(res, 502, result.error || 'LLM query failed');
  res.json({ ok: true, text: result.text });
});

// --- ODT extraction (path or upload) ---
app.post('/api/odt/extract', upload.single('file'), (req, res) => {
  try {
    let buf: Buffer | null = null;
    if (req.file) buf = req.file.buffer;
    else if (req.body?.path) buf = readFileSync(String(req.body.path));
    if (!buf) return fail(res, 400, 'Provide an ODT file upload or a { path }.');
    if (!isValidOdt(buf)) return fail(res, 400, 'File is not a valid ODT (ZIP with content.xml).');
    const text = extractPlainText(buf);
    if (!text) return fail(res, 422, 'Could not extract text from ODT.');
    res.json({ ok: true, text });
  } catch (e) {
    fail(res, 500, (e as Error).message);
  }
});

// --- Import VDD: extract ODT -> LLM -> records (one call) ---
app.post('/api/vdd/import', upload.single('file'), async (req, res) => {
  try {
    let buf: Buffer | null = null;
    if (req.file) buf = req.file.buffer;
    else if (req.body?.path) buf = readFileSync(String(req.body.path));
    if (!buf) return fail(res, 400, 'Provide an ODT file upload or a { path }.');
    if (!isValidOdt(buf)) return fail(res, 400, 'File is not a valid ODT (ZIP with content.xml).');

    const text = extractPlainText(buf);
    if (!text) return fail(res, 422, 'Could not extract text from ODT.');

    const result = await queryText(getSettings(), text, VDD_SYSTEM_PROMPT);
    if (!result.ok) return fail(res, 502, result.error || 'LLM extraction failed');

    const records = parseLLMJsonResponse(result.text || '');
    res.json({ ok: true, records, raw: result.text, extractedChars: text.length });
  } catch (e) {
    fail(res, 500, (e as Error).message);
  }
});

// --- Config parse ---
app.post('/api/config/parse', (req, res) => {
  const { paths } = req.body ?? {};
  if (!Array.isArray(paths) || !paths.length) return fail(res, 400, 'paths[] is required');
  try {
    const items = parseConfigFileItems(paths.map(String));
    res.json({ ok: true, items });
  } catch (e) {
    fail(res, 500, (e as Error).message);
  }
});

// --- Verify ---
app.post('/api/verify', async (req, res) => {
  const { records, configPaths } = req.body ?? {};
  if (!Array.isArray(records)) return fail(res, 400, 'records[] is required');
  if (!Array.isArray(configPaths) || !configPaths.length) return fail(res, 400, 'configPaths[] is required (Document Link resolution needs a config file)');
  try {
    const items = parseConfigFileItems(configPaths.map(String));
    const result = await verify(records as VddRecord[], configPaths.map(String), items);
    res.json({ ok: true, records: result.records, configItemCount: result.configItemCount });
  } catch (e) {
    fail(res, 500, (e as Error).message);
  }
});

// --- Bulk checksum ---
app.post('/api/checksum', async (req, res) => {
  const { paths } = req.body ?? {};
  if (!Array.isArray(paths) || !paths.length) return fail(res, 400, 'paths[] is required');
  try {
    const results = await Promise.all(
      paths.map(String).map(async (p: string) => {
        const h = await hashFile(p);
        return { path: p, ...h };
      })
    );
    res.json({ ok: true, results });
  } catch (e) {
    fail(res, 500, (e as Error).message);
  }
});

app.listen(PORT, () => {
  console.log(`VDD Audit server listening on http://localhost:${PORT}`);
});
