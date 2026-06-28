# VDD Audit System — Web Port Design

**Date:** 2026-06-28
**Status:** Approved

## Goal

Port the Qt 5.14.2 desktop app (`VDDAuditSystem`) to a web application, **keeping the
Qt app fully intact**. Add a landing page. Faithfully reproduce the core workflow and
the three feature tabs.

## Architecture: Client + Backend

Chosen because it is the only model that preserves the desktop app's killer feature —
reading files from **local disk and UNC network shares** (`\\server\share`), hashing
them, and comparing — while keeping the LLM API key **server-side** (never exposed to
the browser).

```
VDD_CI_Verification/
├── src/ ...                      ← existing Qt app (UNTOUCHED)
├── release/ ChecksumHasher.exe   ← existing
└── web/                          ← NEW
    ├── package.json              (root scripts: dev/build for both)
    ├── server/                   (Node.js 24 + Express + TypeScript)
    └── client/                   (React + Vite + TypeScript)
```

### Backend (Node.js + Express + TypeScript)

Ports the C++ logic 1:1:

| Endpoint | Method | Ports from | Purpose |
|---|---|---|---|
| `/api/settings` | GET/POST | `AIConfigManager` (QSettings) | Read/update gateway URL, token, temp, maxTokens, model. Token stored server-side in `settings.json`, never returned to client. |
| `/api/llm/models` | POST | `discoverModels()` | List models (Gemini static list, Anthropic static, OpenAI `/v1/models`). |
| `/api/llm/query` | POST | `queryText()` 3-branch | Proxy LLM call (Gemini / Anthropic / OpenAI-compatible). |
| `/api/odt/extract` | POST | `OdtExtractor` + `ZipReader` | Accept `{ path }` (backend reads) OR multipart upload → returns structured plain text. |
| `/api/config/parse` | POST | `parseConfigFileItems` / `parseCSV` / `parseXLSX` | Parse CSV/XLSX config → CI items (fileName, version, expectedMd5, documentLink, component). |
| `/api/verify` | POST | `resolveFilePathForRecord` + `ChecksumWorker` + status logic | Resolve paths (local + UNC), hash (MD5/SHA-1/CRC32), compare, return per-record statuses. |
| `/api/checksum` | POST | `ChecksumWorker` | Bulk hash a list of file paths (Bulk Checksum tab). |

**Hashing:** Node `crypto` (MD5, SHA-1) + ported IEEE-802.3 CRC32 table (identical output
to `crc32.cpp`). Lowercase hex; comparison is case-insensitive (both sides lowercased,
`0x` stripped) — matches `cleanHash` in `onHashFinished`.

**ODT extraction:** use `fflate.unzipSync` to get `content.xml`, then port
`buildStructuredText()` (table → `cell | cell` rows wrapped in `---TABLE---` markers)
verbatim — this is what makes the LLM extraction reliable.

**Verify status rules** (ported exactly):
- `md5Matches = !hasExpectedMd5 || expMd5 == calcMd5`
- `crcMatches = !hasExpectedCrc || expCrc == calcCrc`
- both → `MATCH`; else `MISMATCH`; no file resolved → `MISSING`.
- Config items absent from VDD become `CONFIG_ONLY` records (hashed, no VDD comparison).

### Frontend (React + Vite + TypeScript)

Dark theme matching `stylesheet.qss` (bg `#0A0E1A`, indigo `#6366F1`, glass-morphism
panels). Components:

- **Landing page** — hero, feature highlights (3 tabs), "Launch App" CTA, link to Qt
  desktop build. NEW.
- **HomeTab** — config CSV path input, Import VDD (path or drag-drop), Verify All,
  Export CSV; `StatisticsDashboard` (TOTAL/MATCH/MISMATCH/MISSING/CONFIG/PENDING +
  success rate + filter + CRC32 toggle); `AuditTable` with collapsible per-row hash
  detail.
- **BulkChecksumTab** — add file paths, compute MD5/SHA-1/CRC32, progress, results table.
- **DocumentReviewTab** — ODT path → extract text → LLM quality/CI review.
- **SettingsDialog** — gateway URL, access token (user-entered), temperature, max
  tokens, model selector + Discover Models.

### Data flow (Home audit)

1. User sets config CSV path + imports VDD `.odt`.
2. `/api/odt/extract` → structured text → `/api/llm/query` (with extraction system
   prompt) → JSON array of CIs → records (PENDING).
3. User clicks Verify → `/api/config/parse` + `/api/verify` → backend resolves paths,
   hashes, compares → records with MATCH/MISMATCH/MISSING/CONFIG_ONLY + reasons.
4. Stats + table update. Export CSV downloads results.

### Build / run

- `web/server`: `npm run dev` (tsx watch) on `:3001`.
- `web/client`: `npm run dev` (Vite) on `:5173`, proxies `/api` → `:3001`.
- `web/package.json`: `npm run dev` runs both concurrently.
- Qt build (`mingw32-make`) is entirely separate and unaffected.

## Phasing

1. **Phase 1** — Backend core + Landing + Home audit workflow (import → verify → export).
2. **Phase 2** — Bulk Checksum tab.
3. **Phase 3** — Document Review tab.

## Non-goals (YAGNI)

- No auth/multi-user (internal single-operator tool, mirrors desktop).
- No DB (settings in `settings.json`; records are per-session in browser state).
- No real-time push; progress is per-request/polling.
