# Validation — 0.1.5

Run `Runtime/python/Scripts/python -m pytest Tests -q` after setup and installation
of pytest. The 26 automated tests cover bridge contracts, provider fixtures, profiles,
permissions, multiple conversations and task ownership.

Windows Editor builds were checked on Unreal 5.3 through 5.8. Native browser,
conversation, multiple-conversation and unified-settings automation was executed
on 5.3 and 5.8. Reproduce builds with:

```powershell
./Scripts/Build.ps1 -EngineRoot C:/UnrealEngines
```

Real conversation checks covered Codex, Claude, Antigravity and an isolated Ollama
instance. OpenCode was unavailable; Claude API and OpenRouter were tested with
protocol fixtures and missing-configuration checks, not live paid API calls.
Local inference uses existing separately licensed weights. No Linux/macOS Editor
validation is claimed. Embedded Unreal tool wiring remains a separate integration
from external MCP access. Local reports are generated under Reports/ and are not
included in source distributions.

## 0.1.6 preview — UE 5.3

- 28 Python tests pass, including owned policy changes without session replacement
  and contextual asset references passed as metadata rather than file imports.
- Live Codex App Server: one native conversation rejects a write in read-only,
  writes in workspace-write, then rejects again after restoring read-only.
- Web smoke checks include the capture action, native screenshot chip and separate
  reference/file payloads in addition to Markdown/media regressions.
- UE 5.3 compilation and ContextAttachments / UnifiedSettings automation pass.
  Native checks cover separate tasks for two host views, real Content Browser asset
  drag-operation handling, a real viewport PNG, isolated drafts, settings navigation
  and preservation of the other view on closure. The version footer is visible.
- New native features were not compiled or executed on UE 5.4–5.8 yet.

## 0.1.7 — offline assistant runtime, UE 5.3

- 28 existing Python provider/native-contract tests passed. The 10 Windows bootstrap
  tests cover check-without-extraction, exact portable Python/dependencies, reuse from
  another plugin copy, real plugin-local installation, concurrent installation,
  cancellation before/during extraction, checksum/path rejection and repair without
  replacing an existing installation.
- UE 5.3 Win64 Editor compilation passed. Native OfflineRuntimeSetup automation passed
  with no warnings/errors: missing-runtime check, Shared default, explicit install,
  verification/reuse, browser initialization and a real SDK session using the packaged
  companion. Test storage is isolated under the example project's Saved directory;
  no production shared installation was created by this automation.
- Bundle: 35,268,936 compressed bytes; 82,516,219 extracted bytes; 1,948 files inside
  one archive. Python 3.13.16, CyToolsCore 0.10.0 and exact dependencies are recorded
  in bundle.json. Archive/inventory hashes, component licenses and absence of machine
  direct_url metadata / bytecode caches were checked.
- First-use setup supplies progress, Later/cancellation and Settings > Runtime.
  New native screenshots are local Saved artifacts, not repository/package content.
- This change was not built/run on UE 5.4–5.8 or non-Windows platforms. It supplies a
  reference integration; existing user plugins were not migrated by this validation.

## 0.1.8 — MIT distributions and integration notices

Core 0.10.1 and companion 0.1.8 wheels/sdists contain the full MIT text and
License-Expression: MIT metadata. The rebuilt offline archive includes both MIT
notices, exact dependency versions and verified file/archive inventories.
The reference plugins include root LICENCES folders; build/install scripts and
BuildPlugin filters preserve them. Integration guides require the same in target plugins.

Validation: 135 Core tests and 38 companion tests passed, including 10 Windows offline
bootstrap tests. The final archive's install/import/reuse path was checked again.
Core scaffold delivery checks verify the generated Tool paths after adding its MIT notice.
No C++ or UI changes are included; prior engine build/UI evidence above remains applicable,
without claiming new compilations for UE 5.3–5.8 in this licensing update.
