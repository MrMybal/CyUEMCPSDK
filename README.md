# CyUEMCPSDK 0.1.8

Unreal SDK modules using **CyToolsCore 0.10.1** contracts and **integrated provider transports**
with no CyAIConnectorLab dependency. Separate minimal host plugin: **CyUEMCP**.

- `CyUEMCPCore`: operation registry and common C++ contract.
- `CyUEMCPBridge`: authenticated loopback bridge to the Core runtime/MCP adapter.
- `CyUEAssistant`: tabbed chat with Markdown, file/image inputs, media/3D previews, connection settings and logs.
  See [Assistant UI](Docs/AssistantUI.md) for setup and exact format support.
- `CyUELocalInference`: optional owned llama.cpp process using existing GGUF weights.
- `CyUEMCP`: editor context, selection, task-owned JSON export and transactional actor label change.

Supported build targets are UE **5.3, 5.4, 5.5, 5.6, 5.7, 5.8**, Windows x64 Editor.
See [provider setup](Docs/Providers.md) and [validation](Docs/Validation.md) for exact evidence and limits.

## Installation

The optional assistant includes an offline ZIP with Python and its dependencies.
At first opening it checks for a compatible shared installation, then offers to
install if needed. **Shared user storage is the default**; plugin-local storage is
optional. See [Assistant runtime](Docs/AssistantRuntime.md).

For development builds, clone CyToolsCore alongside this repository:

```powershell
./Scripts/Setup.ps1 -CoreRoot ../CyToolsCore
./Scripts/Build.ps1 -EngineRoot C:/UnrealEngines
./Scripts/Install.ps1 -Project D:/MyProject/MyProject.uproject -Version 5.8
```

The installer refuses to overwrite existing plugin folders. Enable **CyUEMCP** in
Unreal's Plugins window and restart. Open the assistant through the host integration
API described in [AssistantContexts](Docs/AssistantContexts.md). Global Window menu
entries are disabled by default; that guide documents an optional developer example menu.

The assistant connects its local bridge automatically after runtime setup. Use
**Settings > Runtime** to install, repair, check or choose Shared/Local storage.
Installation needs your explicit click and works offline without existing Python.
Custom Python/script paths are optional under **Providers > Advanced**.

Default profiles cover **Codex App Server / CLI**, **Claude CLI**, **Antigravity CLI**,
**OpenCode CLI**, **Ollama**, **Claude API**, **OpenRouter API** and **local llama.cpp**.
In **Settings > Providers**, select a connector and use **+ New profile**
for another configuration. Choose a model, **Save profile**, then **Test connection**.
Startup checks discover installation/login without generating messages. **Permissions**
controls project editing; **Advanced** contains executable/transport/bridge overrides.

Select a profile in a new chat and send directly. The actual connector and model stay
visible. Use the conversation tab strip's **+** for independent conversations;
switching or closing one does not cancel another. Model and connector changes apply to new conversations; saved permissions also apply
to existing chats after the current turn.
Errors appear in chat and native status/logs. No permanent Copy message buttons.

**Settings > Connection logs** provides selectable logs, Copy all and export.
**Settings > Providers > Local · llama.cpp** configures an existing llama-server/GGUF, port and GPU
layers (0 = CPU), with Start/Stop. Models are not copied. No engine/model downloads
or license acceptance occur implicitly. Select a `local` profile after loading.

The single **Assistant** window contains Chat and Settings. Conversation tabs have
rounded Chrome-style tops and integrated close controls. Settings are native Slate
pages inside that same window; local engine setup belongs to its provider profile. There is no user-facing Tools tab. Operations remain accessible
to AI clients through SDK/MCP. See [AssistantUI](Docs/AssistantUI.md).

## MCP for external agents

The editor writes its connection file under LocalAppData/CyUEMCPSDK. While Unreal and the companion
are running, generate a configuration without copying any secret into it:

```powershell
./Scripts/WriteMCPConfig.ps1 -EditorConnection "$env:LOCALAPPDATA/CyUEMCPSDK/editor-PID.connection.json" -Output ./mcp.local.json
```

The generated stdio command runs `Bridge/run.py --editor <exact file> --mcp` with
the verified installed runtime Python. The client uses **CyToolsCore's official MCP adapter**, including
job polling, automatic file imports, export and FinishTask. Each editor instance has
its own connection file; use the file for the current editor process ID.

Embedded conversation and external MCP access are distinct capabilities. The current
embedded connector baselines do not automatically grant embedded Codex/Claude tool access.

## Adapt to an existing project

Start with [the integration guide](Docs/Integration.md) and its mandatory
[project adaptation, assistant and installation requirements](Docs/ProjectIntegration.md).
The reusable [agent prompt](Docs/AgentPrompt.md) points to these instructions.
Existing project features must be reused. The assistant and local inference are optional. When included, their activation should
reuse existing components or offer only the needed downloads, with the applicable
license/account messages. The common optional activation/download flow remains to
be completed or connected to an existing installer; MCP-only projects do not need it.

## Develop another plugin

Read [Docs/Integration.md](Docs/Integration.md) and [Docs/Architecture.md](Docs/Architecture.md).
For an existing plugin, follow [UpdateExistingPlugin](Docs/UpdateExistingPlugin.md) and embed the C++ sources in its existing modules; do not add a separate SDK plugin dependency. Register operations under your plugin owner. Do not copy
the Python runtime, provider transports, MCP implementation or schemas.

`Plugins/` contains distributable sources; `Builds/UE*/Plugins/` contains per-engine
binaries; `Bridge/` contains the companion; `Runtime/` is the private Python environment;
`Reports/` contains local test evidence. User tokens and running-editor metadata stay
in the user's LocalAppData, outside project source control.

Contextual instances, asset reference drops and viewport screenshots: [AssistantContexts](Docs/AssistantContexts.md). Version 0.1.6 UI is currently validated on UE 5.3 only.

## License

Copyright (c) 2026 Cyberalien. CyUEMCPSDK is licensed under [MIT](LICENSE).
Commercial embedding is permitted with SDK license/copyright notices retained;
your own plugin code need not use MIT. Third-party components retain their licenses.
See [Licensing](Docs/Licensing.md) for distributions and the offline runtime.
