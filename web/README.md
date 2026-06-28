# VDD Audit System — Web Edition

A web port of the Qt desktop `VDDAuditSystem`, built as a **React (Vite) frontend +
Node.js (Express) backend**. The Qt desktop app remains fully intact in the parent
directory — this is an additional, independent edition.

## Why a backend?

The desktop app's core feature is reading files from **local disk and UNC network
shares** (`\\server\share`), hashing them, and comparing against a VDD. Browsers can't
do that, so a small Node backend handles file access, hashing, ODT extraction, config
parsing, verification, and proxies the LLM calls (keeping the API token server-side).

## Prerequisites

- Node.js 18+ (developed on Node 24)

## Install

```bash
cd web
npm run install:all      # installs root, server, and client deps
```

## Run (development)

```bash
cd web
npm run dev              # starts server (:3001) + client (:5173) together
```

Then open http://localhost:5173. The Vite dev server proxies `/api` → `:3001`.

You can also run them separately:

```bash
npm run dev:server       # Express on :3001
npm run dev:client       # Vite on :5173
```

## Configure the LLM

Open **Settings** in the app (top-right) and enter:

- **Gateway URL** — e.g.
  - Gemini: `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent`
  - Anthropic: `https://api.anthropic.com`
  - Any OpenAI-compatible endpoint (OpenRouter, Ollama, Azure, internal proxy)
- **API Access Token** — your key (stored server-side in `web/server/settings.json`,
  never returned to the browser)
- **Model** — pick via *Discover Models* or type one

Optionally seed defaults via `web/server/.env` (copy from `.env.example`).

## Features

| Tab | What it does |
|---|---|
| **Home** | Import a VDD `.odt` (path or drag-drop) → LLM extracts CI items → set a config CSV/XLSX path → **Verify All** resolves each file from its Document Link, hashes it (MD5/SHA-1/CRC-32), and reports MATCH / MISMATCH / MISSING / CONFIG_ONLY. Export results to CSV. |
| **Bulk Checksum** | Paste file paths (local or UNC), compute MD5/SHA-1/CRC-32 for each. |
| **Document Review** | Extract an ODT's text and run an LLM quality + CI-consistency review. |

## File access model

Paths you type (config file, target directory, VDD) are read by the **server**, so they
must be reachable from the machine running the backend — exactly like the desktop app's
"browse to folder" flow, and this is what keeps UNC network-share verification working.
The VDD `.odt` may also be drag-dropped (uploaded as bytes).

## Production build

```bash
cd web
npm run build           # type-checks + builds server (dist/) and client (dist/)
npm run start           # runs the built server
```

(Serve the client `dist/` behind your web server, or extend the Express app to serve it.)
