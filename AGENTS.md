# CyUEMCPSDK
Use the installed CyToolsCore Python runtime and schemas; do not copy or fork them.
Read README.md and Docs/Architecture.md. Keep Editor code out of Runtime modules.
No business parameter supplies authenticated identity. No arbitrary eval/console commands.
Mutations use transactions where meaningful; capability metadata must state undo/save behavior.
A task cleanup must never delete assets integrated into the Unreal project.
Compile each supported UE version; record missing or untested versions honestly.
Python tests: python -m pytest Tests. Build: Scripts/Build.ps1.

Provider transports belong to cyue_mcp.providers. No CyAIConnectorLab dependency.

Read Docs/ProjectIntegration.md before integrating an existing project. Reuse its features; do not duplicate assistants, MCP servers or installers. Assistant/local inference are optional. Only selected optional components need setup/download flows with applicable license/account messages. Keep the plugin functional without them and distinguish documented requirements from implemented capabilities.

Read Docs/AssistantUI.md: compact conversation only in CEF; native Settings pages inside a single Assistant window, multiple persistent profiles, real startup probes and retests, first-send auto-open. No user-facing Tools page. Preserve a tested native chat round trip.

Read the 0.1.4 conversation UX contract in Docs/AssistantUI.md. Ship connector JSON presets, keep English as the default, distinguish profiles from independent chat tabs, show the real transport/model, and enforce snapshotted permissions. One companion per editor; a separate SDK task and provider session per conversation. No permanent Copy message buttons.

Read Docs/Providers.md. Local models belong under Providers. Use Chrome-style native tabs and register only Assistant in the editor Window menu; do not reintroduce separate settings/local/log windows.

Target Fab plugin migrations embed the assistant/MCP/providers in the target existing plugin/modules, not separate CyUEMCPSDK/CyUEMCP dependencies. Read Docs/UpdateExistingPlugin.md section 3; separate reference plugins are development examples only.
