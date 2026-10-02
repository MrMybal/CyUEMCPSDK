# Integrating an Unreal plugin

Read this guide and the CyToolsCore docs before adding operations. The reference
Python contracts remain in the installed Core package: do not fork their schemas.
Connector implementation: `Bridge/cyue_mcp/providers/`, shipped inside this SDK.

For an already integrated plugin, start with [UpdateExistingPlugin.md](UpdateExistingPlugin.md).

## Mandatory project adaptation

First read [ProjectIntegration.md](ProjectIntegration.md). Audit existing features,
reuse the project's services, assistant, MCP and installers, and fill its integration
matrix before creating replacements. Embedded assistance and local inference are
optional: keep the plugin usable without them. If included, offer simple activation,
reuse compatible installations and fetch only missing components, showing their
licenses and required account/acceptance messages. The guide distinguishes current
SDK capabilities from the optional setup paths still to implement.

## Included integration and module scope

For the requested Fab plugins, embed the adapted assistant/MCP/provider components
inside the target plugin's existing modules and resources. Ship one existing .uplugin;
do not require separate CyUEMCPSDK or CyUEMCP plugins or create a separate SDK module.
Read section 3 of UpdateExistingPlugin.md for migration, packaging and coexistence.
CyUEMCPSDK is the source/convention reference. The separate plugins in this repository
are development examples, not the required packaging for the target product.

Editor services belong in the target's Editor module. Keep existing runtime business
modules free of Editor dependencies. Preserve Core contracts; package or provision the
official Python packages without a developer-machine dependency or a forked runtime.

## Contextual assistant views

Read [AssistantContexts.md](AssistantContexts.md) when including the assistant.
Create one view per host/toolkit context and pass its plugin/document identity;
never share one chat widget across distinct plugin panels. Preserve asset reference
drops, the viewport capture action, the version footer and synchronized permissions.

## Operation registration

The following example uses the reference SDK module names. When embedding it, adapt
the registry access, headers and symbols to the target plugin's internal services.
On the game thread in StartupModule:

```cpp
#include "CyUERegistry.h"
auto Descriptor = CyUEJson::Parse(TEXT(R"JSON({
  "id":"my_plugin.inspect",
  "name":"Inspect my plugin",
  "description":"Describe the currently selected plugin object without modifying it.",
  "inputSchema":{"type":"object","properties":{},"additionalProperties":false},
  "outputSchema":{"type":"object","properties":{"ready":{"type":"boolean"}},
                  "required":["ready"],"additionalProperties":false},
  "permissions":[],
  "x-cy-unreal":{"modifiesProject":false,"undoSupported":false,"savesToDisk":false,
                 "inputFiles":[]}
})JSON"));
FString Error;
const bool Registered = FCyUEMCPCoreModule::Get().Registry.Register(
    TEXT("MyPlugin"), Descriptor.ToSharedRef(), false,
    [](const FCyUEContext& Context, const TSharedRef<FJsonObject>& Parameters) {
        auto Output = MakeShared<FJsonObject>();
        Output->SetBoolField(TEXT("ready"), true);
        return FCyUEResult::Ok(Output);
    }, Error);
// Handle registration failure; duplicate IDs are an error.
```

In ShutdownModule, unregister your owner if Core is still loaded:
`FCyUEMCPCoreModule::Get().Registry.UnregisterOwner(TEXT("MyPlugin"));`.
Restart the companion after adding/removing operations; the Core descriptor is
snapshotted when it starts. An unloaded operation returns OperationUnavailable.

## Complete business contracts

Describe every supported input, output, enum, default and constraint. The editor's
full-operation form and AI/MCP submit the same operation and parameters. Keep the
native validation for object state, files, permissions and invariants: a JSON schema
cannot prove an actor still exists or an asset can be edited.

`Register(..., true, ...)` requires `ue.write` and the panel's modification gate.
Additional descriptor permissions are preserved. Declare what changes, whether
Undo is supported, and whether data is saved. Use FScopedTransaction and Modify()
where meaningful. Never silently save a map or delete an asset as a side effect of
an inspection operation. The sample rename operation only accepts selected actors.

Current native delegates run synchronously on the game thread. They must be short,
bounded editor operations. They are advertised as async SDK jobs but **not** as
cancellable native handlers. A long inference or import must use a proper worker
extension with proven lifetime management; do not block the game thread or advertise
cancellation that cannot stop it. An HTTP timeout retries the same receipt ID rather
than repeating a mutation. Reservations stay held while native completion is unknown.

## Files and composability

List every artifact input path in `x-cy-unreal.inputFiles`, e.g.
`["sourceMesh", "references.*.image"]`. The companion configures Core's automatic
imports for these fields; external absolute paths need no manual copy. Do not mark
model weights, executables, export destinations or Unreal object paths as artifacts.

Create transient output paths with `Context.OutputPath("result.json", Path)` and
return paths in a typed output schema. AI/MCP sessions are temporary: export desired
outputs before FinishTask. Imported copies and job workspaces are deleted only after
workers stop. Original inputs and model weights are never task artifacts.

Assets incorporated into the Unreal project are durable project modifications,
not files that FinishTask may remove. Return Unreal object paths separately from
transient filesystem paths. `ue_export_selection` demonstrates task-owned output.

## Conversations and connector extension

Cloud/agent connectors use `cyue_mcp.providers`, included in the SDK package.
CyAIConnectorLab is an example only, never an installation or runtime dependency.
Provider command builders and parsers belong to the SDK companion, not Slate. The catalog distinguishes
implemented transports from descriptors awaiting implementation. Provider support
for tools does not prove that embedded tool calling is available in this adapter.
The current embedded baseline is conversation; external agents access Unreal through
MCP independently.

Conversation ownership derives from Core's authenticated client and task/session.
Concurrent sends on one conversation return SessionBusy. FinishTask/disconnect
closes its native connector process; persistent standalone sessions close when the
editor closes (dock tabs share one controller). No implicit account-wide "last conversation" resume is used.

The local extension in `Bridge/cyue_mcp/local.py` implements the same Transport
contract for an authenticated loopback llama.cpp endpoint. It is an extension here,
not a claim about an external connector library. The owning Unreal module loads an existing
GGUF; it keeps it alive until Stop local model or editor shutdown. Local HTTP
cancellation waits for the bounded in-flight request; local token streaming and
native session resume are not implemented in 0.1.0.

## Required validation

Build each target engine version with Scripts/Build.ps1. Run Python tests and the
real editor/MCP smoke test. For each additional operation test valid parameters,
invalid parameters, permission refusal, returned outputs and cleanup. Test Undo and
save effects for modifications. Only claim the engine/platform combinations actually
compiled and exercised. The 0.1.0 delivery targets Windows x64 Editor, UE 5.3–5.8;
Linux, macOS and packaged game targets are not validated.


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

## MIT distribution (0.1.8)

Read [Licensing.md](Licensing.md). Preserve the SDK MIT notice in each embedded
plugin, alongside third-party notices. Use Core 0.10.1 or later and the 0.1.8 bundle;
replace ZIP, bundle.json, Licenses.txt and LICENSE-CyUEMCPSDK.txt together.
Do not only edit a notice in an older GPL runtime; rebuild or replace its Core wheel.


## Required LICENCES folder in embedded plugins

Read [Licensing.md](Licensing.md). Every integration must ship the complete SDK MIT
notice at <Plugin>/LICENCES/CyUEMCPSDK-MIT.txt. Include Core/runtime component notices
when bundled, preserve the target plugin's own license, and verify LICENCES is present
in the delivered source/binary/Fab package, not only the checkout.
