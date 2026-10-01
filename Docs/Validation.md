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
