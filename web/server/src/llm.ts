// LLM gateway proxy — ports AIConfigManager (queryText + discoverModels) with the
// same three-branch routing: Gemini / Anthropic direct / OpenAI-compatible.
// Uses Node's global fetch. The access token never leaves the server.

import type { Settings } from './types.js';

function isGeminiUrl(url: string): boolean {
  return url.includes('googleapis.com') || url.includes('generativelanguage');
}

function isAnthropicUrl(url: string): boolean {
  return url.includes('anthropic.com');
}

/** Build the Gemini generateContent URL with the selected model substituted in. */
function buildGeminiQueryUrl(gatewayUrl: string, model: string): string {
  const modelsIdx = gatewayUrl.indexOf('/models/');
  if (modelsIdx >= 0) {
    const base = gatewayUrl.slice(0, modelsIdx);
    return `${base}/models/${model}:generateContent`;
  }
  let base = gatewayUrl;
  if (base.endsWith('/')) base = base.slice(0, -1);
  return `${base}/models/${model}:generateContent`;
}

export interface QueryResult {
  ok: boolean;
  text?: string;
  error?: string;
}

/** Send a text query to the configured LLM gateway. */
export async function queryText(
  s: Settings,
  promptText: string,
  systemPrompt: string
): Promise<QueryResult> {
  if (!s.accessToken) {
    return { ok: false, error: 'No API access token configured.' };
  }

  let gw = s.gatewayUrl;
  if (gw.endsWith('/')) gw = gw.slice(0, -1);

  let url: string;
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  let body: unknown;

  if (isGeminiUrl(gw)) {
    url = buildGeminiQueryUrl(gw, s.targetModel);
    headers['X-goog-api-key'] = s.accessToken;
    const json: Record<string, unknown> = {
      contents: [{ role: 'user', parts: [{ text: promptText }] }],
      generationConfig: { maxOutputTokens: s.maxTokens, temperature: s.temperature },
    };
    if (systemPrompt) {
      json.system_instruction = { parts: [{ text: systemPrompt }] };
    }
    body = json;
  } else if (isAnthropicUrl(gw)) {
    url = `${gw}/v1/messages`;
    headers['x-api-key'] = s.accessToken;
    headers['anthropic-version'] = '2023-06-01';
    const json: Record<string, unknown> = {
      model: s.targetModel,
      max_tokens: s.maxTokens,
      temperature: s.temperature,
      messages: [{ role: 'user', content: promptText }],
    };
    if (systemPrompt) json.system = systemPrompt;
    body = json;
  } else {
    // OpenAI-compatible gateway (OpenRouter, Ollama, Azure, internal proxies).
    let baseUrl = gw;
    if (!baseUrl.endsWith('/v1/chat/completions')) {
      baseUrl += baseUrl.endsWith('/v1') ? '/chat/completions' : '/v1/chat/completions';
    }
    url = baseUrl;
    headers['Authorization'] = `Bearer ${s.accessToken}`;
    const messages: Array<{ role: string; content: string }> = [];
    if (systemPrompt) messages.push({ role: 'system', content: systemPrompt });
    messages.push({ role: 'user', content: promptText });
    body = { model: s.targetModel, max_tokens: s.maxTokens, temperature: s.temperature, messages };
  }

  let resp: Response;
  try {
    resp = await fetch(url, { method: 'POST', headers, body: JSON.stringify(body) });
  } catch (e) {
    return { ok: false, error: `Error transferring ${url} - ${(e as Error).message}` };
  }

  const raw = await resp.text();
  if (!resp.ok) {
    return { ok: false, error: `Error transferring ${url} - server replied: ${resp.statusText || raw.slice(0, 200)}` };
  }

  let doc: any;
  try {
    doc = JSON.parse(raw);
  } catch {
    return { ok: false, error: 'Failed to parse LLM response JSON.' };
  }

  const text = extractText(doc, raw);
  if (text) return { ok: true, text };
  return { ok: false, error: 'Unexpected LLM response format.' };
}

/** Extract the text payload across Gemini / Anthropic / OpenAI shapes. */
function extractText(doc: any, raw: string): string {
  // Gemini: candidates[0].content.parts[0].text
  if (Array.isArray(doc?.candidates) && doc.candidates.length) {
    const parts = doc.candidates[0]?.content?.parts;
    if (Array.isArray(parts) && parts.length && typeof parts[0]?.text === 'string') {
      return parts[0].text;
    }
  }
  // Anthropic: content[] with {type:'text', text} or {type:'tool_use', input}
  if (Array.isArray(doc?.content)) {
    for (const val of doc.content) {
      if (val?.type === 'text' && typeof val.text === 'string') return val.text;
      if (val?.type === 'tool_use' && val.input) return JSON.stringify(val.input);
      if (typeof val === 'string') return val;
    }
  }
  // OpenAI-compatible: choices[0].message.content
  if (Array.isArray(doc?.choices) && doc.choices.length) {
    const msg = doc.choices[0];
    if (typeof msg?.message?.content === 'string' && msg.message.content) return msg.message.content;
    if (typeof msg?.content === 'string' && msg.content) return msg.content;
  }
  // Direct text field
  if (typeof doc?.text === 'string' && doc.text) return doc.text;
  // Fallback: raw JSON
  return raw;
}

/** Discover available models for the configured gateway. */
export async function discoverModels(s: Settings): Promise<{ ok: boolean; models?: string[]; error?: string }> {
  let gw = s.gatewayUrl;
  if (gw.endsWith('/')) gw = gw.slice(0, -1);

  if (isGeminiUrl(gw)) {
    return {
      ok: true,
      models: [
        'gemini-2.5-flash',
        'gemini-2.0-flash',
        'gemini-2.0-flash-lite',
        'gemini-1.5-flash',
        'gemini-1.5-flash-8b',
        'gemini-1.5-pro',
        'gemini-flash-latest',
      ],
    };
  }

  if (isAnthropicUrl(gw)) {
    return {
      ok: true,
      models: [
        'claude-3-5-sonnet-20241022',
        'claude-3-opus-20240229',
        'claude-3-haiku-20240307',
      ],
    };
  }

  // OpenAI-compatible /v1/models
  try {
    const url = `${gw}/v1/models`;
    const headers: Record<string, string> = { 'Content-Type': 'application/json' };
    if (s.accessToken) headers['Authorization'] = `Bearer ${s.accessToken}`;
    const resp = await fetch(url, { headers });
    if (!resp.ok) return { ok: false, error: `Network error (${resp.status}): ${resp.statusText}` };
    const doc: any = await resp.json();
    let models: string[] = [];
    if (Array.isArray(doc?.data)) models = doc.data.map((m: any) => m?.id).filter(Boolean);
    else if (Array.isArray(doc)) models = doc.filter((m: any) => typeof m === 'string');
    if (models.length) return { ok: true, models };
    return { ok: false, error: "Parsed reply but no model objects with 'id' found." };
  } catch (e) {
    return { ok: false, error: (e as Error).message };
  }
}
