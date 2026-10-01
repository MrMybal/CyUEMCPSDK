
## Included delivery requirement

For the target Fab plugins, the assistant, MCP and provider resources must be included
inside each existing plugin and its existing modules. No separate SDK plugin/module
installation or activation is required. The reference repository's separate example
plugins are not the target delivery architecture. This requirement supersedes older
instructions to add CyUEMCPSDK/CyUEMCP as external plugin dependencies. Follow section 3
of UpdateExistingPlugin.md and verify the target plugin works without those examples.

Pour un plugin déjà intégré, utiliser d'abord [UpdateExistingPlugin.md](UpdateExistingPlugin.md) : commit de sauvegarde, audit et migration préservant l'existant.

# Prompt pour intégrer CyUEMCPSDK à un projet

Intègre CyUEMCPSDK dans le projet <NOM_DU_PROJET>, situé dans <CHEMIN_DU_PROJET>.
Lis et applique <SDK_ROOT>/Docs/Integration.md et tous les guides qu'il
référence, en particulier ProjectIntegration.md.

Commence par auditer les fonctionnalités déjà présentes. Adapte et réutilise les
services métier, le MCP, l'assistant, les connecteurs et les installateurs existants ;
ne crée pas de doublons et ne supprime pas les options spécifiques du projet.
Expose ses fonctions réelles avec le même contrat complet dans l'interface et le MCP.
L'assistant embarqué et l'inférence locale sont optionnels : le plugin et le MCP
restent utilisables sans eux. S'ils sont retenus, rends leur ajout et leur activation
simples avec les modules communs. Réutilise les installations présentes ; propose
uniquement les téléchargements nécessaires au composant choisi, avec ses licences,
liens officiels et messages de connexion ou d'acceptation lorsqu'ils sont requis.
Prévois « Plus tard » et l'annulation sans bloquer le reste du plugin. Aucun
téléchargement ou consentement automatique lors de la découverte de l'option.
Raccorde et teste les appels d'outils de l'assistant lorsqu'il doit agir sur le projet.

Utilise CyToolsCore et les connecteurs intégrés à CyUEMCPSDK. Aucune dépendance à
CyAIConnectorLab. Respecte les fichiers de tâche et les assets persistants Unreal.
Implémente les éléments manquants nécessaires ; ne confonds pas exigences documentées
et fonctionnalités déjà disponibles. Livre Docs/CyUEMCPIntegration.md, les preuves
et les limites exactes pour les versions Unreal demandées entre 5.3 et 5.8.


## Assistant UI integration (0.1.3)
Read AssistantUI.md. Reuse an existing rich chat when available. Otherwise use the
adapted CyUEAssistant with independent contextual views and native Settings pages.
Preserve Markdown, selectable/copyable messages, attachment drafts, paste/drop,
image inputs when implemented, and media/mesh previews. Do not substitute a long
vertical diagnostic form. Keep adapter input capabilities distinct from preview
capabilities. Use assistant_send files through automatic task imports, propagate
unsupported-format errors and clean private copies; do not ask users to copy files
manually. Do not require CyMD or CyAIOrchestrator as runtime dependencies.

## Native assistant layout and connection contract (0.1.3)

Keep only the conversation in the embedded web renderer. Use native Settings pages inside Assistant
for Providers (including local engine profiles) and selectable/copyable logs. No
user-facing raw Tools page. Reuse one companion bridge per editor, with a separate authenticated SDK session
and provider conversation per chat tab. Multiple persisted profiles must support startup checks
without inference and manual retests. Use assistant_probe for real discovery/login;
listing connector descriptors is not a connection test. First Send opens the selected
profile's conversation automatically, reports opening errors visibly, and submits once.
Keep profile histories isolated, retain Markdown/file/media support, and validate a real
native-panel round trip before declaring chat functional. Adapt existing project UIs
instead of creating duplicate assistants.


## Conversation UX and connector data (0.1.4)

Use the English default UI. Ship explicit per-connector data and default profiles with
automatic discovery. Show actual connector transport and model in every conversation.
Profiles and conversations are different: provide multiple independent chat tabs.
Keep common connection fields simple; move paths/transport overrides to Advanced.
Provide a Permissions section whose controls are enforced by the backend, scoped and
applied between turns to existing chats after backend acknowledgement; no cosmetic permission toggles. Keep a separate explicit
editor-wide MCP write gate. Do not place permanent Copy message buttons in the feed.
Use text selection/context actions, frame-batched streaming and non-forcing scrolling.
Test real concurrent conversations, switching, closing one without affecting another,
and read-only refusal versus authorized editing before claiming them functional.


## Assistant views and Providers (0.1.6)

Read Providers.md, UpdateAssistantInterface.md and AssistantContexts.md. Provide
assistant and new-instance actions through the existing host UI only. Do not add
global Assistant/New assistant window entries or generic global tab spawners by default. Use compact native
Chrome-style conversation tabs with integrated close controls, a Settings gear at
the right and native settings subpages; do not add a separate Chat/Settings row. Providers includes CLI/API
connections AND local inference; do not create a separate Local Model settings window.
Keep one data definition per connector, default profiles, discovery and manual retests.
Persist endpoint and environment-variable key names, never API keys in profile data.
MCP is an operation/tool connection, not implicitly a conversation/model provider.
Only expose implemented transports as usable; installed/account/API status must come
from actual probes. Test HTTP terminal events, failure handling and private history.
