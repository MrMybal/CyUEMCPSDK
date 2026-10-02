# Assistant UI — 0.1.6

Open the assistant from its owning plugin/toolkit. No global Window menu entry is added by default. The packaged local web UI
runs inside Slate's WebBrowser/CEF. Node, CyMD, CyAIOrchestrator and CyAIConnectorLab
are not runtime dependencies. CyAIOrchestrator's draft attachments, paste/drop and
media cards were examined as a UX reference; its application is not imported.

## Conversations and settings

The UI is English by default. **Assistant** has a horizontal conversation tab strip with rounded native
Chrome-style tabs, integrated close icons, middle-click closing and a flat + control.
Use **+** to start a separate conversation. Each tab owns its provider session,
messages, drafts, attachments and running request. Switching does not restart it;
closing one tab finishes only that tab's task. One editor companion is shared.
The connector/transport and model identity remain visible, together with the active
read-only/editing policy. Resolved model names replace Provider default when reported.

**Settings** is inside Assistant. Its **Providers** page contains connection profiles
and local models together, with three sections:

- **Connection**: profile name, model selection, startup check, Save and Test connection.
- **Permissions**: read-only by default, optional project-file editing for Codex/Claude,
  and a separate editor-session gate for registered Unreal mutations via external MCP.
- **Advanced**: custom model ID, executable override, transport override and bridge paths.

The plugin ships one JSON data file per connector under Resources/Connectors. Default
profiles are Codex App Server, Codex CLI, Claude CLI, Antigravity CLI, OpenCode CLI,
Ollama, Claude API, OpenRouter API and local llama.cpp. See [Providers.md](Providers.md). Existing
custom profiles migrate without being removed. Executables use the existing native
provider discovery; detected paths do not need to be entered. Model catalogs are
queried without generation where supported. Codex CLI uses its installed App Server
for model discovery only, then executes the selected model through CLI JSONL. Claude
CLI has no model catalog in this adapter: use Provider default or a custom model ID.
No unavailable transport is presented as usable. App Server is not falsely labeled
as an implementation of the separate official SDK transport.

Create additional profiles with **+ New profile**, based on the selected connector.
Each has a Test connection action and an optional startup check. Connection tests do
not create conversations or send inference requests. New conversations snapshot model and connector settings. Saved permissions synchronize
to existing chats between turns, with the effective policy acknowledged by the companion.
An active turn finishes under its original policy; history is preserved.
Unsaved permissions remain draft values and are not persisted by background checks.

Copy selected text normally. Right-click a message for Copy message. No persistent
copy button is inserted beneath every reply. Code-block copy appears on hover/focus.
Rendering is grouped per animation frame and follows new output only when already
near the bottom, so reading earlier messages is not interrupted.

**Settings > Providers > Local · llama.cpp** contains the engine controls.
**Settings > Connection logs** contains the shared log. There are no additional
Connections/Local Model/Logs entries in Unreal's Window menu, nor a default global Assistant entry.
Logs merge conversations with a tab identifier and support selection, Copy all and
export. Profiles persist in the project's user Editor INI without account secrets.
Authentication reuses the installed CLI account. Weights are never copied by profiles.

### Effective permissions

The service takes its project directory from the authenticated editor descriptor,
not from assistant_open business input. Codex uses read-only or workspace-write
sandbox policy; workspace writes target that project. The App Server workspace
sandbox explicitly disables network access; CLI network policy remains subject to
the installed CLI configuration. Claude allows Read/Glob/Grep; enabling edits adds scoped
Edit/Write, with dontAsk and no shell/web tools. Local inference and HTTP API adapters expose no file tools. Antigravity uses its
CLI plan mode and sandbox flags, identified as Plan mode in chat; OpenCode denies
tool permissions. These do not claim the Codex filesystem sandbox contract.
Unreal MCP mutation permission is an editor-wide, session-only gate and is separate
from provider filesystem rights. Embedded Unreal tool wiring remains a separate
integration; this settings page does not claim to supply it.

Sources: [Codex App Server](https://developers.openai.com/fr-FR/docs/app-server),
[Claude CLI](https://code.claude.com/docs/en/cli-reference). Actual enforcement tests
are recorded in Reports/live-permissions.json for both providers.

## Markdown and media

Headings, bold/italic, Discord-style __underline__, ~~strikethrough~~, ||spoilers||,
quotes, lists, fenced code with language highlighting, tables, links and emoji shortcodes.
Markdown images appear as clickable placeholders; remote image data loads only after clicking. Raw HTML/scripts are escaped. Spoilers can be revealed with mouse or keyboard.
This is a chat Markdown renderer, not Discord's full protocol or a CyMD document editor.

Attach files with the button, drag/drop, or clipboard image paste. Draft chips show
preparation and allow removal before sending. Media cards appear in the conversation;
images/video can be enlarged. Images, browser-supported video/audio, and self-contained
GLB, OBJ and STL have previews. 3D supports orbit, pan, wheel zoom and light presets.
OBJ external MTL/textures and GLTF external bundles are not loaded by this preview.
CEF codec availability varies by engine; unsupported playback reports an error.

## Real inputs to assistants

`assistant_send` accepts `text` (may be empty with files) and `files: [{path, name?}]`.
Paths go through Core automatic imports (`files.*.path`) and authenticated task ownership.
The UI stages uploaded bytes privately; successful sends release the UI staging copy.
Closing a conversation removes its staging copies and finishes its own SDK task;
closing the editor closes all remaining conversations.
Original source files are not changed. Normal SDK task cleanup owns imported files.
An editor crash can leave UI upload staging files under Saved/CyAssistant/Uploads;
these are not model weights. Provider-owned transcript retention is separate.

- UTF-8 text/code and extracted text from PDFs are included in the prompt.
- Codex app-server: real localImage inputs; opaque files passed as explicit read-only
  original-file references for its local agent tools, not advertised as video perception.
- Claude CLI: real image blocks over stream-json input, along with text.
- Local llama.cpp: text only in this adapter. Images require a vision adapter/projector
  and are rejected explicitly here; they are never silently dropped.
- No arbitrary video/audio/native 3D perception is claimed. A viewer is not model input.

Limits: 8 files per message, 32 MiB per uploaded file, 20 million pixels per image,
180000 extracted attachment characters combined, Claude normalized image 5 MiB.
Oversized files are rejected, not silently truncated. GIF input sends its first frame
with an explicit note. Scanned PDFs without text require page images.

`assistant_open` also exposes optional `access` (read-only/workspace-write), `executable`, `transport`, and timeout (5–600 s).
Returned attachmentCapabilities describe adapter support; model-specific refusal still
propagates as a visible error. No fallback resubmits a potentially billed request.

## Build and tests

Run `npm ci` then `npm run build` inside UI only when modifying frontend sources.
Compiled Resources/Assistant ships with the plugin; Scripts/Install.ps1 copies it.
Run `node smoke.cjs` in UI for browser UI regressions and screenshots.
Run private Python `-m pytest Tests -q`; `Tests/live_attachments.py` sends synthetic
image/text fixtures to real Codex and Claude accounts (may consume provider usage).
Native Unreal automation: `CyUEMCP.Assistant.BrowserReady` checks CEF and bridge.
`CyUEMCP.Assistant.LiveConversation` checks native profile creation, missing-executable
diagnosis, successful retest, and real Codex response rendered after the web Send action.
The live test consumes provider usage; it runs against the isolated host project.
See Reports for actual tested versions; build success alone is not a GUI check.

References: https://developers.openai.com/fr-FR/docs/app-server and
https://code.claude.com/docs/en/cli-reference; local CyAIOrchestrator cyai/media.py,
cyai/web/media.js and cyai/connectors.py were inspected for UX and transport examples.

## Compact layout preview (UE 5.3)

The conversation strip is the only top row: 28-pixel tabs and a fixed Settings
gear at the right edge. Settings replaces the content in the same window; use
the back arrow or the gear to return without recreating the conversation.
This layout preview is compiled and checked on UE 5.3 only, pending review.

## Contextual views and editor attachments (0.1.6, UE 5.3 preview)

Use the owning plugin/toolkit’s new-instance action for another independent window. Integrators use
`ICyUEAssistantModule::CreateAssistant` for a dock/toolkit and `OpenAssistant` for a
window. See [AssistantContexts.md](AssistantContexts.md). Every view owns separate
chat tasks, drafts, uploads and provider histories. The companion remains shared.

Drop Content Browser assets onto the chat: reference chips carry the object path,
name and class, without copying .uasset files. The camera button beside + captures
the active editor viewport as a temporary PNG attachment; it is sent only with Send.
Remove, successful send and view closure clean staged screenshots. Model image
support still depends on the selected connector. The footer displays the actual
plugin descriptor version in small text, including on Settings pages.


## Offline assistant setup (0.1.7)

Read [AssistantRuntime.md](AssistantRuntime.md). Ship the compressed offline archive
with portable Python, the official Core package and companion dependencies. At first
assistant opening, check compatible installations without extracting anything; propose
installation only if missing/damaged and wait for the explicit Install action.
Shared user storage is the default; plugin-local storage is optional. Preserve Later,
cancellation, progress, license notices and Settings > Runtime. Share immutable code
by exact archive hash, never profiles, tokens, conversations or task files. Adapt the
bootstrap to the target plugin's existing Editor module and private settings/service
names. No developer venv, loose Python tree, SDK checkout path or external SDK plugin
is an end-user prerequisite. Test isolated plugins and simultaneous shared installs.
