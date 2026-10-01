# Contextual assistant integration

Embed these services in the existing plugin/modules as described in UpdateExistingPlugin.md.
The reference module's public API is CyUEAssistant.h. Adapt its symbols to the host;
do not add a mandatory separate SDK plugin dependency for Fab deliveries.

```cpp
FCyAssistantContext Context;
Context.Id = TEXT("MyPlugin:MyDocument");
Context.Label = TEXT("My plugin / My document");
Context.Plugin = TEXT("MyPlugin");
Context.Asset = TEXT("/Game/Materials/MyMaterial.MyMaterial");
auto Chat = ICyUEAssistantModule::Get().CreateAssistant(Context);
// Put Chat in this document's existing dock tab. Another call creates an independent view.
// Or open a separate window:
ICyUEAssistantModule::Get().OpenAssistant(Context);
```

For a changing active document, provide the optional resolver callback returning
FCyAssistantContext. Capture host/toolkit objects weakly and handle their closure.
It is read on the game thread at send time; set File for a relevant source file
and Asset for a relevant Unreal object. Context is displayed and sent as metadata,
not silently read from disk, not a system instruction and not a permission grant.
Use a stable plugin/document Id; separate calls always own separate chats, even
when their Id matches. Do not reuse one widget in two parents.

Do not select a global last-open asset for all plugins. Each host supplies its own
context. Project-level windows use project context unless their host supplies a
resolver. Asset references remain object paths; screenshot PNGs use the existing
private upload/task cleanup flow. Neither exports nor duplicates asset packages.

Saved profile permissions update all chats using that profile after any active turn.
Codex and Claude apply the policy on their next native turn without resetting history.
Model/transport changes still require a new chat. Unsupported providers retain their
actual capabilities; an asset reference does not give an assistant Unreal tools.

Validate two host contexts, independent drafts/tasks, close-one/keep-other behavior,
permissions in both directions, asset chips and a real viewport capture. Current
new-feature validation targets UE 5.3 only; do not claim the other builds were updated.

## Host-owned entry points

No global Window > Assistant or New assistant window menu is registered by default.
Expose CreateAssistant/OpenAssistant through the owning plugin or document toolkit.
When migrating, remove only that integration's old menu registrations. Namespace
host commands/tab identifiers; never remove or replace another plugin's entries.

The reference example has a developer-only opt-in in the project's user Editor INI:
`[CyUEMCPSDK.Assistant]` / `EnableGlobalMenu=True` (restart required).
Leave it absent/false in normal integrations and do not enable it in shipped defaults.
Opening through the host API and independent contextual views do not need this option.
