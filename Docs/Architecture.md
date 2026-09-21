# CyUEMCPSDK architecture

Version 0.1.5. Target: Unreal Engine 5.3–5.8.
A version is validated only after its actual compilation and runtime checks.

## Shared foundations

- **CyToolsCore 0.9.0** owns manifests, parameter validation, authenticated identities,
  sessions, jobs, resource reservations, task files and the official MCP transport.
- **CyUEMCPSDK** contains its own provider transports in `Bridge/cyue_mcp/providers`:
  discovery, authentication, sessions, normalized events, process IO and descriptors.
  CyAIConnectorLab was an implementation example only. It is not imported, installed,
  searched for or needed at build/runtime. No external connector library is required.
- **CyToolsCore** remains the shared tool runtime as requested; provider CLIs and an
  optional local model engine are the actual execution backends.

## Unreal modules

| Module | Scope |
|---|---|
| CyUEMCPCore | Runtime registry, portable operation descriptors and handler context |
| CyUEMCPBridge | Editor authenticated loopback endpoint for the Python SDK companion |
| CyUEAssistant | Editor conversation panel consuming normalized connector events |
| CyUELocalInference | Optional local inference integration; separate from cloud connectors |
| CyUEMCP | Minimal host plugin: editor context, selection and explicitly gated edits |

Other Unreal plugins depend on the Core module and register their operations under
an owner name. They unregister on shutdown. The host loads optional Editor modules
explicitly. A Runtime module must not import Slate, Editor, Python, MCP or AI runtimes.

## Conversations

`Bridge/cyue_mcp/connectors.py` uses `create_transport`, `descriptors`, detection,
connection, native sessions, send/cancel/disconnect from the integrated provider module. Connector
choices come from descriptors, never an Unreal hardcoded list of provider commands.
Unsupported transports stay visible as unavailable. Declared capabilities and test
observations remain separate. A descriptor is not an implemented connector.

One editor companion is shared by the settings controller and all conversation tabs.
Each tab owns a separate SDK session, provider conversation and web renderer. Closing
a tab finishes its task without cancelling another tab. Connector presets come from
Resources/Connectors JSON; profile settings are snapshotted when opening a conversation.

Each conversation belongs to an authenticated transport identity. Clients cannot
claim ownership with a business parameter. One active turn per conversation;
parallel conversations use separate transports. Codex App Server remains alive
across turns. There is no implicit resume of the last account session and no fallback
that resends a possibly billed request. Deltas and snapshots remain distinct events.
Retained host event history is bounded. Cancellation precedes waiting for the actual
worker exit and disconnect. Only owned processes are closed.

The current embedded conversational baselines do not enable arbitrary MCP tools for
embedded agents. Exposing Unreal through CyTools MCP and granting an embedded agent
tool access are separate capabilities and require separate validation. No capability
is reported as tested merely because a provider supports it elsewhere.

Local inference uses the same internal Transport contract via `local.py`.
Ollama, OpenRouter and Claude API use HTTP conversation transports with application-owned
history. CodeBuddy, Command Code, Devin and unimplemented SDK/ACP alternatives remain
explicitly unavailable. See Providers.md for precise coverage.
The UI has one Assistant dock tab. Settings and all provider/local/log pages are
children of it; switching settings preserves the conversation widgets.

## File and operation ownership

AI task inputs are automatically imported into private job workspaces using Core.
Generated intermediate files belong to the task; export wanted files before
FinishTask. Unreal assets integrated into a project are persistent project changes,
not temporary files to delete during task cleanup. An operation declares this effect,
its permissions, transaction/undo support and whether it saves to disk.

## Rich assistant panel

See [AssistantUI.md](AssistantUI.md). Only a packaged local page receives the native UObject bridge. Navigation and popups are blocked; explicit HTTP(S) links open externally. Markdown content is sanitized. Attachment bytes are never sent through console logs. The Core remains UI-free.
