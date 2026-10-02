# Mettre à jour l’assistant : interface et instances contextuelles — 0.1.6

Ce document est le point d’entrée pour une IA qui met à jour un plugin déjà intégré.
Lire [UpdateExistingPlugin.md](UpdateExistingPlugin.md) pour la sauvegarde, l’audit,
les dépendances incluses et la préservation des fonctions existantes. Lire aussi
[AssistantUI.md](AssistantUI.md) et [AssistantContexts.md](AssistantContexts.md).

## 1. Sauvegarde et périmètre

Faire un commit local de sauvegarde avant les modifications, après vérification des
fichiers. Sans Git, faire une sauvegarde datée vérifiable. Ne pas écraser les travaux
existants, ni publier automatiquement. Noter le commit dans Docs/CyUEMCPIntegration.md.

Adapter l’intégration présente : conserver les opérations métier, les providers,
les traductions, les profils, les installations optionnelles et le câblage des outils
Unreal déjà fonctionnel. Inclure les changements dans le plugin et ses modules
existants ; aucun nouveau plugin/module SDK séparé à installer. Adapter les noms
et les ressources au plugin cible, sans chemins vers le poste du développeur.

## 2. Interface compacte

- Une seule rangée supérieure : onglets de conversation à gauche, engrenage Settings
  à droite. Pas de rangée supplémentaire Chat / Settings.
- Onglets natifs façon Chrome, environ 28 px de haut, largeur compacte, croix intégrée,
  clic central pour fermer et + pour une nouvelle conversation dans cette instance.
- Settings s’ouvre dans le même panneau ; retour au chat sans détruire ses widgets.
  Providers et Connection logs restent les pages des réglages. Local est un provider.
- Garder le connecteur, le modèle et les droits effectifs lisibles. Afficher le contexte
  de l’hôte et, en petit en bas, la version réellement intégrée de l’assistant.
  Si la version du plugin métier est différente, stocker séparément la version du
  composant assistant : ne pas afficher la version métier comme version du SDK.
- Anglais par défaut, Markdown et médias conservés, pas de Copy message permanent.
- À côté du + des pièces jointes, ajouter le bouton caméra du viewport : aperçu et
  pièce jointe préparée, sans envoi automatique au modèle.

## 3. Instances et contexte de chaque hôte

Distinguer trois notions : un profil de connexion, une instance de l’assistant,
et les conversations de cette instance. Un profil peut servir à plusieurs instances
sans partager leur historique ni leur brouillon.

Chaque panneau de plugin ou éditeur de document crée sa propre vue avec l’équivalent
adapté de CreateAssistant(Context). OpenAssistant(Context) ouvre une autre fenêtre.
Prévoir l’ouverture d’une nouvelle instance dans l’interface du plugin/toolkit hôte.
Ne pas ajouter de bouton Assistant ou New assistant window global dans Window par
défaut. Retirer les anciennes inscriptions de cette intégration uniquement. Les
commandes et identifiants d’onglets de l’hôte doivent être propres au plugin.
Ne pas placer le même widget Slate dans plusieurs parents et ne pas utiliser un
singleton de conversation commun à tous les plugins.

Le contexte contient Id, Label, Plugin, Asset et File selon les informations réellement
disponibles. Utiliser des identifiants stables plugin/document. Chaque hôte fournit
son contexte ; ne pas choisir le dernier asset ouvert globalement pour tous les chats.
Un résolveur facultatif relit le document actif de cet hôte au moment de l’envoi.
Capturer les toolkits faiblement et gérer leur fermeture. Consulter les exemples C++
dans AssistantContexts.md et l’API de référence Public/CyUEAssistant.h.

Afficher le contexte et l’envoyer à l’assistant comme métadonnées du message.
Un chemin de fichier dans le contexte ne signifie ni lecture automatique ni droit
supplémentaire. Si le contenu est nécessaire, utiliser les pièces jointes ou les
outils autorisés. Le contexte ne remplace pas l’identité authentifiée du transport.

Chaque conversation garde sa tâche SDK, sa session provider, ses messages, brouillons,
pièces jointes, génération et annulation propres. Fermer une instance termine ses
propres tâches sans arrêter les autres. Mutualiser le companion compatible dans son
périmètre existant, jamais les sessions ; entre plugins autonomes, préserver le
namespacing des services, ports, fichiers de connexion et ressources.

## 4. Assets et capture du viewport

Intercepter le drag-and-drop natif du Content Browser. Ajouter une puce de référence
contenant le chemin objet Unreal, le nom et la classe. Transmettre references dans
assistant_send ; ne pas convertir cette référence en chemin de fichier, copier un
.uasset ou exporter le mesh automatiquement. L’utilisateur peut retirer la puce.

La caméra capture le viewport actif avec repli explicite sur le viewport de niveau
si nécessaire. Un hôte spécialisé adapte ce choix à son viewport métier. Signaler
l’absence de viewport lisible. Préparer un PNG privé, montrer l’aperçu et n’envoyer
l’image qu’avec Send. Réutiliser les limites des pièces jointes et le nettoyage des
copies temporaires après retrait, envoi réussi ou fermeture. Préserver les assets
originaux. Un modèle sans entrée image doit retourner une erreur compréhensible.

## 5. Permissions réellement actualisées

Migrer ensemble le panneau natif et le companion. Les cases seules ne suffisent pas.
Réutiliser assistant_set_access ou son équivalent dans le projet : identité propriétaire,
validation de la politique et refus de changement pendant un tour actif. Synchroniser
les permissions enregistrées vers les chats utilisant ce profil après le tour courant.

Pour Codex/Claude, appliquer la nouvelle politique au tour suivant sans recréer la
session native ni perdre l’historique. Afficher la politique confirmée par le backend.
En cas d’échec, expliquer le problème et ne pas envoyer sous une politique différente
de celle demandée. Protéger les valeurs enregistrées contre les sondes asynchrones
et les copies obsolètes des profils présentes dans d’autres fenêtres.

Les modifications de modèle/transport restent pour les nouvelles conversations.
Conserver la distinction entre droits fichiers du provider et mutations Unreal via MCP.
Ne pas donner artificiellement des outils d’édition aux providers qui n’en ont pas.

## 6. Sources à comparer, pas à remplacer aveuglément

Dans Plugins/CyUEMCPSDK/Source/CyUEAssistant :
Public/CyUEAssistant.h, Private/CyUEAssistant.cpp, CyAssistantNative.inl,
CyChatWorkspace.inl et CyChromeTabs.inl. Adapter les dépendances Editor du Build.cs.

Dans Bridge : cyue_mcp/service.py (contrats), connectors.py (sessions/droits) et
les adaptateurs réellement utilisés. Dans UI : main.js ; reconstruire app.js après
modification et embarquer les ressources et notices tierces. Ne pas remplacer les
adaptations métier du plugin par les exemples de la référence.

## 7. Validation et livraison

Commencer sur UE 5.3 pour cette évolution ; ne porter aux autres moteurs que selon
la demande de l’utilisateur. La validation locale 5.3 ne prouve pas 5.4–5.8.

1. Compiler puis ouvrir la vraie interface ; vérifier ergonomie compacte et version.
2. Ouvrir deux hôtes avec contextes différents et deux conversations ; contrôler
   isolation des messages/brouillons/pièces jointes et fermeture indépendante.
3. Vérifier qu’un changement de document actualise uniquement le contexte de son hôte.
4. Tester lecture seule → écriture → lecture seule sur la même conversation, sans
   perdre l’historique, et enregistrer les droits pendant un tour actif.
5. Déposer un asset : puce et référence reçue par l’assistant, aucun package copié.
6. Capturer le vrai viewport : aperçu, retrait, envoi, nettoyage ; tester le refus
   d’image par un provider incompatible et l’absence de viewport.
7. Vérifier les opérations MCP métier déjà présentes et la coexistence des plugins.

Mettre à jour Docs/CyUEMCPIntegration.md avec les changements, la version du composant,
les tests réellement exécutés, les limites et les captures. Ne déclarer aucune
validation réelle à partir de mocks uniquement. Préserver les fonctions spécifiques.


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
