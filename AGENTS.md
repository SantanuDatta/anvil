# agent.md — Anvil (Qt App) Codebase Guide

This repository contains **Anvil**, a C++/Qt desktop application that orchestrates local development services (PHP, Node, Nginx, Databases, DNS/hosts) and manages “sites” end-to-end. The architecture is layered to keep UI, orchestration, and system-level operations cleanly separated.

---

## 1) High-Level Architecture

**Entry**
- `src/main.cpp`
  Application bootstrap (Qt app init, MainWindow creation, wiring managers/services).

**Core Foundation**
- `src/core/ConfigManager.*`
  App configuration management (paths, settings, persistence, defaults).
- `src/core/ServiceManager.*`
  Central service orchestration (start/stop/restart/status, dependencies, lifecycle coordination).

**System Services**
- `src/services/*Service.*`
  Encapsulates system-level behavior for each managed service:
  - `BaseService.*` — common service interface + shared helpers
  - `PHPService.*` — PHP version mgmt + runtime control
  - `NodeService.*` — Node.js/nvm mgmt
  - `NginxService.*` — Nginx config + enable/disable/reload
  - `DatabaseService.*` — MySQL/PostgreSQL mgmt
  - `DnsService.*` — `/etc/hosts` management

**Business Logic Orchestration**
- `src/managers/SiteManager.*`
  Owns “site” lifecycle: create, delete, enable, disable, link runtime versions, update configs.
- `src/managers/VersionManager.*`
  Installed versions discovery + switching (PHP/Node/etc). Marked “Done”.
- `src/managers/ProcessManager.*`
  Central process execution, process tracking, safe kill/restart. Marked “Done”.

**Data Models**
- `src/models/Site.*` — site definition/state
- `src/models/PHPVersion.*` — PHP version metadata
- `src/models/Service.*` — service status + metadata (running/stopped/error/etc)

**Utilities**
- `src/utils/Logger.*` — structured logging
- `src/utils/Process.*` — process execution primitives (sync/async, stdout/stderr, exit codes)
- `src/utils/FileSystem.*` — file IO, templates, atomic writes, permissions
- `src/utils/Network.*` — network helpers (ports, host checks, etc)
- `src/utils/DistroDetector.*` — Linux distro detection + environment quirks
- `src/utils/resources/` — icons/scripts/templates (currently TODO but expected to back services)

**UI Layer (TODO)**
- `src/ui/Theme.*` — UI theme system
- `src/ui/MainWindow.*` — main app window
- `src/ui/TrayIcon.*` — system tray integration
- `src/ui/dialogs/*Dialog.*` — Add Site wizard, Settings UI

**Tests (Partial)**
- `src/tests/integration/*` — integration tests
- `src/tests/unit/` — unit tests (TODO)

---

## 2) Core Runtime Flow (Expected)

A typical user action (e.g., “Add Site” or “Switch PHP Version”) flows like:

1. **UI** triggers intent:
   - `AddSiteDialog` / `MainWindow` emits a request.
2. **Manager** coordinates the business operation:
   - `SiteManager` validates inputs, updates `Site` model, coordinates template generation.
3. **Service orchestration** ensures underlying services reflect the new state:
   - `ServiceManager` calls relevant `*Service` to update config and reload.
4. **System-level effects** (files/processes) are done via utilities:
   - `FileSystem` writes templates, `Process` runs system commands, `Logger` records steps.
5. **Models** are updated and returned to UI for display.

---

## 3) Boundaries / Responsibilities

### UI (`src/ui`)
- No shell commands.
- No direct file writes to system config.
- Talks to managers/services via clean interfaces and emits signals/slots.

### Managers (`src/managers`)
- Own use-cases and workflows (site lifecycle, version switching, process orchestration).
- Should not contain distro-specific logic; delegate that to `services` or `utils`.

### Services (`src/services`)
- The single place where service semantics live (nginx reload, php-fpm start, hosts updates).
- Uses `utils/Process`, `FileSystem`, `Network`, `DistroDetector`.

### Core (`src/core`)
- App-wide configuration and orchestration.
- `ServiceManager` is the “conductor”; it should not duplicate service logic.

### Utils (`src/utils`)
- Reusable primitives only.
- Keep them free of “business meaning” (e.g., no “site” logic).

### Models (`src/models`)
- Data containers + minimal invariants.
- No heavy orchestration inside models.

---

## 4) Key Artifacts and Where They Live

**Templates & Config Generation**
- Expected location: `src/utils/resources/templates/`
- Nginx vhost templates, php-fpm pool templates, DB configs, etc.

**Scripts**
- Expected location: `src/utils/resources/scripts/`
- helper scripts for distro quirks, permission fixes, package checks.

**Icons**
- `src/utils/resources/icons/`

---

## 5) Error Handling & Logging Conventions

**Logging**
- Use `utils/Logger` for all operational logs.
- Prefer structured logs:
  - operation name (e.g., `site.create`, `nginx.reload`)
  - user action context (site name, requested version)
  - outcome (success/failure) + error codes

**Process Execution**
- All external commands go through `utils/Process`.
- Capture:
  - exit code
  - stdout/stderr
  - duration (if available)
- Convert command failures into actionable error messages at manager/service level.

**Filesystem**
- Use `utils/FileSystem` for:
  - atomic writes
  - permission setting
  - directory creation
  - safe delete/rollback patterns

---

## 6) Configuration & State

**ConfigManager responsibilities (expected)**
- location of config file(s)
- reading/writing persistent settings
- defaults + migration if config schema changes
- per-distro overrides (if any) via `DistroDetector`

**Possible state sources**
- On-disk config managed by `ConfigManager`
- service status gathered at runtime via `ServiceManager` -> `*Service` -> `Process/Network`

---

## 7) Testing Strategy

**Integration tests (`src/tests/integration`)**
- Validate end-to-end flows:
  - service discovery
  - version switching
  - process manager behavior
  - config generation + reload commands
- These may require mocking system commands or running in CI with containers.

**Unit tests (`src/tests/unit`)** (TODO)
Recommended unit targets:
- `FileSystem` (atomic write, permissions, path ops)
- `Process` (parsing, timeout handling, output capture)
- `DistroDetector` (distro identification)
- `ConfigManager` (read/write/migrations)
- model validation logic in `Site`, `PHPVersion`, `Service`

---

## 8) Contribution Notes (Practical Guidelines)

**Where to add new functionality**
- New service integration (e.g., Redis): add `src/services/RedisService.*`, wire via `ServiceManager`.
- New site workflow: implement in `SiteManager` and call service layer for side effects.
- UI additions: place dialogs in `src/ui/dialogs`, keep orchestration in managers.

**Avoid**
- UI calling shell commands directly.
- Services writing random files without `FileSystem`.
- Multiple sources of truth for service state (centralize in `ServiceManager` + `Service` model).

---

## 9) Quick Map: “If you need to change X, go to Y”

- App settings storage → `core/ConfigManager.*`
- Start/stop/restart logic → `core/ServiceManager.*` + relevant `services/*Service.*`
- Nginx config generation → `services/NginxService.*` + `utils/resources/templates/`
- PHP versions and switching → `services/PHPService.*` + `managers/VersionManager.*`
- Node versions/nvm → `services/NodeService.*` + `managers/VersionManager.*`
- Database mgmt → `services/DatabaseService.*`
- `/etc/hosts` changes → `services/DnsService.*`
- Running system commands → `utils/Process.*` (and often via `managers/ProcessManager.*`)
- File operations → `utils/FileSystem.*`
- UI screens → `ui/*` and `ui/dialogs/*`

---

## 10) Roadmap Pointers (Based on Current “TODO” Areas)

**UI**
- Implement `MainWindow` as the primary state viewer/controller:
  - Sites list + details
  - Service statuses
  - Quick actions: start/stop/restart, reload Nginx, switch PHP/Node versions
- Implement `TrayIcon` for quick toggles/status indicators

**Resources**
- Populate:
  - `utils/resources/templates` with Nginx + service templates
  - `utils/resources/scripts` with privileged operations helpers (if needed)
  - `icons` with app + tray icons

**Unit Tests**
- Expand unit coverage for utils and core state logic.

---

## 11) Common End-to-End Use Cases (Cheat Sheet)

**Create Site**
- UI → `SiteManager::createSite(...)`
- `FileSystem` writes site root + Nginx config template output
- `DnsService` adds host entry
- `NginxService` enables vhost + reload
- Return updated `Site` model for UI rendering

**Switch PHP Version for Site**
- UI → `SiteManager::setPhpVersion(site, version)`
- `PHPService` ensures version exists/active (or install flow)
- Update templates (php-fpm pool / nginx fastcgi pass)
- Reload/restart relevant services

**Start/Stop All Services**
- UI → `ServiceManager::startAll()` / `stopAll()`
- Each `*Service` handles details and updates `Service` model status

---

## 12) Build & Run (Repo-level)

- Build system: `CMakeLists.txt`
- Artifact output: `build/Anvil` (executable)
- See: `README.md` for the official build/run steps and dependencies.

---
