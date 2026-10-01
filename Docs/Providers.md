# Providers — 0.1.5

Open the assistant from its owning plugin/toolkit, then **Settings > Providers**. A provider profile selects a
connector, implemented transport and model. Duplicate a preset with + New profile,
Test connection to discover status/models, choose the model and save. A new chat
snapshots that profile. Existing chats retain their settings.

| Preset | Transport | Setup and validation |
|---|---|---|
| Codex · App Server | persistent JSON-RPC | Existing Codex CLI account; authenticated probe and model catalog |
| Codex CLI | CLI JSONL | Same native account; model discovery via installed App Server |
| Claude CLI | CLI stream-json | Existing Claude account; native auth check, default or custom model ID |
| Antigravity CLI | agy stream-json | Existing account; models discovered; authentication remains unverified until a turn succeeds |
| OpenCode CLI | CLI JSONL | Installed CLI required, not just desktop app; model catalog, account checked on generation |
| Ollama | HTTP NDJSON | Running server, default http://127.0.0.1:11434; installed models via /api/tags |
| Claude API | HTTPS Messages SSE | ANTHROPIC_API_KEY; authenticated /v1/models catalog |
| OpenRouter API | HTTPS Chat Completions SSE | OPENROUTER_API_KEY; /key verifies the key before /models |
| Local · llama.cpp | owned local HTTP server | Engine/GGUF/CPU-GPU/start-stop controls in this provider's Connection page |

CLI executable discovery is automatic; overrides belong in Advanced. No implicit
CLI/model download or license acceptance occurs. A provider missing its installation,
account or API key reports that state rather than pretending to be connected.

For HTTP providers, Server URL and API key environment variable are profile fields.
Enter the environment variable NAME, not its value. Set its value on the computer and
restart Unreal so the companion inherits it. Each profile may use a different variable.
Ollama needs no key by default; a custom protected endpoint can reference one.
Remote endpoints require HTTPS. Redirects and credentials embedded in URLs are refused.
No key is persisted in profile JSON/INI. Probes never submit generation requests.
Cloud API model choice is explicit; the public OpenRouter catalog alone is not proof
of valid credentials. Test errors include authentication, missing server and rate limits.

The local llama.cpp engine remains one shared editor-owned server. Its current GGUF
and engine controls are visible inside the Local provider; weights are not copied.
Ollama is a separate provider for an existing Ollama service and installed models.

## Permissions and protocol scope

Codex/Claude CLI project-editing policies remain explicit. Antigravity uses plan mode
and sandbox flags; its UI says Plan mode. OpenCode tool permissions are denied. HTTP
providers and local inference implement text conversations, not filesystem/MCP tools.
Text/PDF attachments can be converted to text; image/video input is not advertised for
these new HTTP transports. Streams require a terminal event; incomplete responses do
not enter retained history and are never automatically resent. Cancellation is checked
while reading; stalled network reads can wait until the bounded socket timeout.

MCP exposes tools/operations to an assistant, not a generic language-model chat API.
The editor's MCP mutation gate remains under Permissions. Do not label a CLI as SDK,
or imply embedded Unreal tools are connected merely because a provider supports MCP.
CodeBuddy, Command Code, Devin and alternate SDK/ACP transports from ConnectorLab are
still descriptors, not working providers. The implemented baseline has nine presets.
No ConnectorLab dependency is installed or imported.

## Protocol references

- [Ollama chat](https://docs.ollama.com/api/chat) and [models](https://docs.ollama.com/api/tags).
- [Claude Messages](https://platform.claude.com/docs/en/api/messages/create) and [models](https://platform.claude.com/docs/en/api/models/list).
- [OpenRouter chat](https://openrouter.ai/docs/api/api-reference/chat/create-a-chat-completion), [models](https://openrouter.ai/docs/api/api-reference/models/list-all-models-and-their-properties), [key check](https://openrouter.ai/docs/api/api-reference/api-keys/get-current-key).

See Validation.md for exact fixture and live evidence; implementation is not proof of
an authenticated live test for every provider/account.
