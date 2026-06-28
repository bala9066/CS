# VDD Automated Audit & Verification System

## Project Overview

C++ Qt 5.14.2 desktop application (QMake/MinGW) that parses Version Description Documents (VDD), extracts CI references, and verifies file integrity against a local target directory. Integrates with an internal Anthropic API Gateway for LLM-powered document analysis.

**App name:** `VDDAuditSystem` | **Org:** `DevCorp`
**Entry point:** `src/main.cpp` → `MainWindow` (shows maximized)
**High-DPI:** Enabled (`AA_EnableHighDpiScaling`, `AA_UseHighDpiPixmaps`)

## Build

```bash
mingw32-make -j4          # Build release
./release/ChecksumHasher.exe  # Run
```

**Dependencies:** Qt 5.14.2 (core, gui, widgets, network), zlib (ODT ZIP decompression)
**Output:** `release/ChecksumHasher.exe` + required Qt DLLs
**Build artifacts:** `build/` (MOC/MOC/UIC generated files)

### Makefiles

Three Makefiles exist: project root `Makefile`, `Makefile.Debug`, `Makefile.Release` in `build/`.

## Architecture

```
MainWindow (central QTabWidget, shown maximized)
├── Home tab — original dashboard: import VDD, verify local, export results
├── Bulk Checksum tab — multi-file hash calculation widget (BulkChecksumDock)
└── Document Review tab — ODT review with LLM-powered quality assessment (VddDocumentReviewer)
```

### Tab Layout Details

- **Home tab** (`m_homePage`): Import VDD/CSV/XLSX → AI-powered parsing → Verify against local directory → Export CSV/PDF
- **Bulk Checksum tab** (`m_bulkChecksumDock`): Drag-drop or browse multi-file hash calculation with progress tracking and cancel support
- **Document Review tab** (`m_reviewerPage` → `m_vddDocumentReviewer`): Browse ODT, extract text via OdtExtractor, send to LLM for quality/CI validation

### Core Classes

| Class | Location | Role |
|---|---|---|
| **`AIConfigManager`** | `aiconfigmanager.{cpp,h}` | Singleton, manages API endpoint/token/model settings via QSettings. Handles LLM text queries with `querySourceId` for concurrent components. Dynamic model discovery via internal Anthropic API Gateway. |
| **`BulkChecksumDock`** | `bulkchecksumdock.{cpp,h}` | QWidget dock for multi-file hash calculation. Drag-drop, progress tracking, cancel support. |
| **`VddDocumentReviewer`** | `vdddocumentreviewer.{cpp,h}` | QWidget for ODT document review. Browses ODT, extracts text via OdtExtractor, sends to LLM for quality/CI validation. |
| **`ChecksumWorker`** | `checksumworker.{cpp,h}` | QObject worker that runs in QThread. Computes MD5, SHA-1, CRC32 hashes with cancellation support. Emits `started`, `fileProgress`, `finished` signals per fileId. |
| **`FolderScanner`** | `folderscanner.{cpp,h}` | QObject worker that scans a directory for files matching filters. Emits `foundFiles` and `scanProgress`. |
| **`CRC32`** | `crc32.{cpp,h}` | Static API using IEEE 802.3 polynomial (0xEDB88320) with precomputed 256-word lookup table. |
| **`ZipReader`** | `zipreader.{cpp,h}` | Custom ZIP reader (replaces Qt private QZipReader). Read-only, constructed from file path, used as value object (no `close()`). |
| **`OdtExtractor`** | `odtextractor.{cpp,h}` | Static API: `isValidOdt(path)`, `extractPlainText(path)`. Uses ZipReader internally to decompress and parse ODT XML. |
| **`IconHelper`** | `iconhelper.{cpp,h}` | Centralized icon management. Loads from `:/icons/` resources. Provides `loadIcon()`, `createIconButton()`, `createStatusIcon()`, `getFallbackIcon()`. |
| **`StatisticsWidget`** | `statisticswidget.{cpp,h}` | Real-time dashboard showing match/mismatch/missing/pending/config-only counts, success rate %, filter dropdown, CRC32 toggle checkbox. |
| **`SettingsDialog`** | (inline in `mainwindow.cpp`) | Modal QDialog for AI Gateway settings: endpoint URL, access token, temperature slider, max tokens, model selector with "Discover Models" button. |

### Key Data Structures

**`VddRecord`** (defined in `mainwindow.h`) — Each audit record contains:
- **Identifiers:** `id`, `fileName`, `ciReference`, `version`, `source` ("VDD" or "CONFIG_ONLY")
- **Hashes:** `expectedMd5`, `calculatedMd5`, `calculatedSha1`, `calculatedCrc32`, `expectedCrc32`
- **Local resolution:** `localFileName`, `localCiRef`, `localVersion`, `localFullPath`
- **Config parsing:** `configFileName`, `configVersion`, `configPath`, `configComponent`
- **Path verification:** `configPathExists`, `configFileFoundAtPath`
- **Statuses:** `localStatus` (PENDING/MATCH/MISMATCH/MISSING), `fileStatus` (adds NOT_IN_FILE)
- **Per-check breakdown:** `ciRefCheck`, `compCheck`, `versionCheck`, `md5Check`, `pathCheck`, `crc32Check` (each "PASS"/"FAIL"/"SKIP")
- **Reasons:** `localStatusReason`, `fileStatusReason`

### State in MainWindow

- `m_records` — `QMap<int, VddRecord>` of all audit records
- `m_verifierThreads` — `QMap<int, QThread*>` for active hash workers
- `m_activeHashJobs`, `m_completedHashJobs`, `m_totalHashFiles` — progress trackers
- `m_fileProgressMap` — per-file progress with mutex protection (`m_progressMutex`)
- `m_mainRowToDetailRow`, `m_rowExpanded` — collapsible row management
- `m_csvFilePaths` — queued CSV/XLSX file paths for import
- `m_llmParseMode` — "vdd", "csv", "xlsx", or "xlx"
- `m_tourStep` — guided tour state (-1 = not active)

## Key Files

- `src/mainwindow.cpp` / `.h` — Main window, core workflow (import, verify, export), LLM handlers, table display, settings dialog, guided tour, drag & drop, CSV/XLSX parsing, collapsible rows, action log dock
- `src/aiconfigmanager.cpp` / `.h` — LLM gateway singleton
- `src/bulkchecksumdock.cpp` / `.h` — Bulk hash dock widget
- `src/vdddocumentreviewer.cpp` / `.h` — Document reviewer widget
- `src/odtextractor.cpp` / `.h` — ODT text extraction
- `src/zipreader.cpp` / `.h` — Custom ZIP reader
- `src/checksumworker.cpp` / `.h` — File hash worker (MD5, SHA-1, CRC32)
- `src/folderscanner.cpp` / `.h` — Directory scanner worker
- `src/crc32.cpp` / `.h` — CRC32 checksum implementation
- `src/iconhelper.cpp` / `.h` — Icon loading helper
- `src/statisticswidget.cpp` / `.h` — Stats display widget
- `src/main.cpp` — Application entry point

## Resources & Styling

### Icons (`src/resources/icons/`)
- `checksum.ico` (app icon), `AI.ico`, `export.ico`, `reset.ico`
- SVG set: `import.svg`, `verify.svg`, `browse.svg`, `save.svg`, `success.svg`, `error.svg`, `warning.svg`, `export.svg`, `settings.svg`, `hash.svg`, `refresh.svg`

### Stylesheets (`src/styles/`)
- `stylesheet.qss` — Dark theme (deep blue-black #0A0E1A, indigo #6366F1 accents, glass-morphism group boxes)
- `stylesheet-light.qss` — Light theme variant

### Resource file (`src/resources.qrc`)
- Two `<qresource>` sections: `/styles` and `/icons`
- Icons loaded via `:/icons/<name>` prefix throughout the code

## Code Guidelines

- **No `qDebug()` in production code** — removed from all source files. Use `logMessage()` for user-facing logging in the action log panel.
- **No debug print statements** — clean build, no console spam.
- **LLM query handlers** include `int sourceId` parameter for distinguishing concurrent queries.
- **Async operations** — hashing uses QThread workers; LLM queries use QNetworkAccessManager with source-ID tracking.
- **Custom ZipReader** has no `close()` method — constructed from file path, used as value object.
- **OdtExtractor** uses static methods — `isValidOdt()` then `extractPlainText()`.
- **Version comparison** — normalize by stripping invisible Unicode characters before comparison.
- **File resolution** — use stripped baseName matching (remove `.`, `-`, `_`) for robust matching.
- **CRC32 toggle** — `m_crc32Checkbox` in StatisticsWidget toggles CRC32 hash inclusion; emits `crc32Toggled()` signal.
- **Collapsible rows** — table supports expanding main rows to show detailed hash breakdown (expected vs calculated for MD5, SHA-1, CRC32).
- **Action log dock** — `m_actionLogDock` with `m_actionLogText` (QTextEdit) and `m_actionLogClearBtn` for user-facing audit trail.
- **Guided tour** — interactive walkthrough with `m_tourCard`, `m_tourText`, `m_tourNextBtn`, `m_tourBackBtn`, `m_tourSkipBtn`.

## Platform

- Windows Server 2022, MinGW 7.3.0, Qt 5.14.2
- Build dir: `build/`, Debug: `debug/`, Release: `release/`
- QMake project: `ChecksumHasher.pro`
- Helper script: `CopyOpenSslDeps.bat`
