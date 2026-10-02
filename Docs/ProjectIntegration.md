
## Included delivery requirement

For the target Fab plugins, the assistant, MCP and provider resources must be included
inside each existing plugin and its existing modules. No separate SDK plugin/module
installation or activation is required. The reference repository's separate example
plugins are not the target delivery architecture. This requirement supersedes older
instructions to add CyUEMCPSDK/CyUEMCP as external plugin dependencies. Follow section 3
of UpdateExistingPlugin.md and verify the target plugin works without those examples.

# Adapter CyUEMCPSDK à un projet existant

Ce document est obligatoire pour intégrer le SDK dans un plugin ou un projet Unreal.
Il complète [Integration.md](Integration.md), [ConnectorConventions.md](ConnectorConventions.md)
et [Architecture.md](Architecture.md). Les exigences ci-dessous décrivent la livraison
attendue ; elles ne signifient pas que tous ces parcours sont déjà fournis en 0.1.1.

## 1. Auditer l'existant avant de créer

Lire les instructions du projet, ses modules et ses fonctionnalités réelles. Repérer :
les commandes et services métier, le MCP existant, les connecteurs IA, l'assistant,
les panneaux, les paramètres, les journaux, les téléchargements, les modèles déjà
installés, les tâches asynchrones, les transactions et les tests.

Créer dans le projet un compte rendu `Docs/CyUEMCPIntegration.md` avec cette table :

| Fonction réelle du projet | Service/API existant | Opération SDK/MCP | Interface existante | Adaptation et preuve |
|---|---|---|---|---|
| Une fonction identifiée dans le code | Classe, méthode et fichier | ID stable, entrées et sorties | Panneau ou commande actuelle | Réutilisé, complété, migré ou manquant |

Ne pas remplir cette table avec des fonctionnalités supposées. Chaque fonction
annoncée doit être reliée à une implémentation et à une validation. Documenter les
lacunes et leur cause ; ne pas les masquer en retirant simplement des options.

## 2. Réutiliser les fonctionnalités déjà présentes

- Envelopper les services métier existants avec les handlers SDK. L'interface et le
  MCP appellent la même logique, avec les mêmes validations et options.
- Si le projet possède déjà un assistant, des connecteurs ou une fenêtre MCP,
  adapter ces composants au contrat commun. Ne pas ajouter un deuxième assistant,
  serveur, système de configuration, téléchargeur ou ordonnanceur équivalent.
- Utiliser le plugin hôte minimal CyUEMCP quand aucun hôte équivalent n'existe.
  Un projet qui possède son propre hôte peut intégrer les modules du SDK dans cet
  hôte ; le panneau d'exemple ne doit pas être imposé en double.
- Conserver les fonctions spécifiques, les paramètres avancés, les profils, les
  raccourcis et les comportements utiles. Faire évoluer l'implémentation existante
  si elle couvre seulement une partie du besoin.
- Pour un ancien MCP, conserver les identifiants utiles ou fournir une migration
  explicite/alias vers le handler commun. Ne pas lancer deux handlers concurrents
  qui effectuent la même modification.
- Réutiliser la détection et les installations existantes après vérification de
  compatibilité. Un modèle ou moteur déjà disponible ne doit pas être retéléchargé.

CyAIConnectorLab est uniquement un exemple historique. Ne pas l'importer, l'installer
ou exiger que son dossier/application existe. Les connecteurs sont intégrés dans
`cyue_mcp.providers`. CyToolsCore reste le socle outils/jobs/MCP demandé.

## 3. Adapter les opérations aux capacités du projet

Exposer les vraies fonctions du projet : inspection, création, édition, génération,
import/export, réglages ou traitements réellement disponibles. Les quatre opérations
d'exemple du plugin hôte ne constituent pas à elles seules l'intégration métier.

Déclarer tous les paramètres, types de sorties, contraintes, plateformes, permissions
et ressources. Identifier ce qui est lisible, modifiable, transactionnel, enregistrable
et persistant. Distinguer les chemins d'objets Unreal des fichiers temporaires.
Respecter le game thread, les workers et l'annulation réelle ; ne pas transformer
un traitement long en appel bloquant dans l'éditeur. Conserver les règles de tâches,
imports automatiques, exports et FinishTask du Core.

## 4. Assistant et inférence locale optionnels

Le socle MCP peut être intégré seul. L'assistant embarqué et l'inférence locale sont
des fonctionnalités optionnelles, sélectionnées par le développeur puis activées
explicitement par l'utilisateur. Leur absence ne doit pas empêcher le plugin et ses
fonctions métier de fonctionner. Ne pas télécharger de CLI, moteur ou modèle au
chargement du plugin, à la découverte MCP ou à la simple ouverture des options.

L'ajout doit être simple : réutiliser les modules et connecteurs communs, déclarer
les composants nécessaires à la fonctionnalité choisie et raccorder un point d'entrée
« Activer l'assistant » dans l'interface existante. Ne pas réécrire un système IA ou
un installateur complet pour chaque projet. Une installation existante compatible
peut être sélectionnée sans nouveau téléchargement.

Si l'assistant est inclus, réutiliser le panneau existant ou intégrer CyUEAssistant.
Prévoir :

- Choix du connecteur et du modèle, état détecté/installé/connecté/occupé/erreur,
  paramètres utiles, connexion/déconnexion et test réel de réponse.
- Conversation libre, messages progressifs quand le transport le permet, annulation,
  nouvelle session et reprise exacte quand elle est supportée.
- Contexte du projet contrôlable : sélection, objet actif, paramètres et capacités
  utiles. Montrer ce qui sera transmis ; ne pas envoyer tout le projet implicitement.
- Liste des opérations accessibles et de leurs effets : lecture, modification,
  Undo, sauvegarde, suppression, fichiers produits. Les permissions réelles doivent
  être appliquées par les handlers, pas seulement décrites dans un prompt.
- Journal consultable et copiable, erreurs avec action de résolution, réglages
  persistants sans secrets, anglais par défaut et français sélectionnable.

Si l'assistant doit agir sur le projet, raccorder ses appels d'outils au registre
commun et tester une action réelle. Un chat qui répond correctement n'est pas une
preuve d'accès aux fonctions du projet. Une opération indisponible doit être signalée
comme telle ; aucune réponse de l'assistant ne doit prétendre l'avoir exécutée.
Les agents externes peuvent utiliser le MCP indépendamment du chat intégré.

## 5. Activation simple, composants optionnels et licences

Le téléchargement sert à fournir les composants de la fonctionnalité optionnelle
choisie, avec leurs conditions et messages. Il ne s'agit pas d'imposer un gestionnaire
général de téléchargements à chaque plugin ni d'installer tous les fournisseurs.

Réutiliser le parcours commun ou celui du projet. Présenter une entrée simple dans
les options : « Activer l'assistant », puis le connecteur ou le modèle souhaité.
Détecter d'abord les composants existants. Télécharger uniquement ceux qui manquent
pour ce choix, après l'action de l'utilisateur. « Plus tard » doit laisser le reste
du plugin utilisable ; une option désactivée n'est pas une panne.

### Informations à déclarer pour chaque composant optionnel

Le projet définit les champs précis de son catalogue. Conserver au minimum :
identifiant du composant, fonctionnalité associée, fournisseur, version ou révision,
source officielle, plateforme, emplacement choisi, taille connue ou inconnue,
nom/identifiant de licence, lien vers son texte et indication d'une éventuelle
acceptation ou connexion requise. Distinguer les licences du plugin, du moteur et
du modèle. Afficher les conditions propres au composant ; ne pas inventer une licence
ou déduire les conditions d'un composant à partir de celles d'un autre.

Le lien de téléchargement et le lien des conditions sont accessibles dans le même
parcours. Montrer une case d'acceptation seulement lorsque les conditions applicables
l'exigent. Une licence accessible ne signifie pas systématiquement qu'une acceptation
interactive est nécessaire. Si cette information est inconnue, l'indiquer et fournir
la source au lieu d'afficher une compatibilité ou une autorisation supposée.

### Parcours attendu

1. Fonctionnalité désactivée : expliquer son utilité, sans téléchargement automatique.
2. Activation demandée : proposer le composant choisi ou une installation existante.
3. Composant manquant : afficher provenance, taille/emplacement et licence, puis
   l'action de téléchargement ou d'installation officielle adaptée.
4. Conditions ou compte requis : afficher le message et le lien exacts ; laisser
   l'utilisateur effectuer l'action sur son compte. Ne pas accepter à sa place.
5. Téléchargement : progression, annulation et reprise si disponible ; vérifier les
   informations d'intégrité publiées, sans inventer de hash attendu.
6. Installation terminée : détecter et tester le composant, puis rendre l'option
   utilisable. Ne pas confondre fichier téléchargé et assistant prêt à répondre.

Pour les connecteurs distants, installer seulement le runtime nécessaire au connecteur
retenu et utiliser sa connexion native. Pour le local, distinguer moteur et poids :
un modèle déjà présent peut être enregistré en place. Garder les installations hors
des fichiers de tâche et des sauvegardes de configuration. Utiliser les conventions
de stockage du Core lorsque pertinentes, sans déplacer les poids existants.

### Messages à fournir en anglais et en français

Ces formulations sont des modèles d'interface, pas des affirmations sur une licence
particulière. Remplacer les variables par les informations réelles du composant.

| Situation | English (default) | Français |
|---|---|---|
| Option inactive | Optional AI assistant. Enable it when needed; the plugin works without it. | Assistant IA optionnel. Activez-le si besoin ; le plugin fonctionne sans lui. |
| Composant manquant | To enable {feature}, install {component}. Download size: {size}. | Pour activer {feature}, installez {component}. Taille du téléchargement : {size}. |
| Licence disponible | {component} uses {license}. View its terms before installation. | {component} utilise {license}. Consultez ses conditions avant l'installation. |
| Acceptation exigée | {component} requires acceptance of its terms. Open {license_url}, then check access again. | {component} nécessite l'acceptation de ses conditions. Ouvrez {license_url}, puis vérifiez à nouveau l'accès. |
| Connexion requise | Sign in to {provider} using its official sign-in flow, then retry. | Connectez-vous à {provider} avec son parcours officiel, puis réessayez. |
| Annulation/refus | Setup cancelled. {feature} remains disabled; other plugin features are available. | Configuration annulée. {feature} reste désactivée ; les autres fonctions du plugin restent disponibles. |
| Prêt | {component} is ready. You can now enable {feature}. | {component} est prêt. Vous pouvez maintenant activer {feature}. |

Boutons selon l'état : Activer, Utiliser une installation existante, Voir la licence,
Ouvrir la page officielle, Télécharger/Installer, Vérifier à nouveau, Annuler et
Plus tard. Ne pas afficher une action non implémentée comme disponible.

Une erreur d'accès doit identifier le composant concerné et l'action nécessaire.
Ne jamais demander de token dans la conversation, les journaux ou les configurations.
Ne pas retenter indéfiniment un téléchargement refusé. Conserver seulement les
informations utiles de version et d'état pour reprendre après l'action de l'utilisateur.

## 6. Même état compréhensible par une IA

Lorsque la fonctionnalité optionnelle est intégrée au projet, exposer les opérations réellement implémentées pour diagnostiquer, consulter le
catalogue, installer/télécharger, suivre un job, annuler et vérifier un composant.
Réutiliser le gestionnaire existant et les jobs Core au lieu de créer un chemin
spécial qui contourne la logique de l'interface.

Les identifiants et schémas précis sont définis par le projet. Prévoir des résultats
structurés pour les situations FeatureDisabled, MissingComponent, AuthenticationRequired,
LicenseAcceptanceRequired, InsufficientDiskSpace, UnsupportedPlatform et Ready,
ou leur équivalent cohérent dans le contrat du projet. Retourner la cause et
l'action suivante, avec le lien concerné si nécessaire, sans secret. Une IA doit
pouvoir reprendre après la connexion ou l'acceptation, sans recopier les fichiers
elle-même et sans recommencer un téléchargement déjà terminé. L'IA ne doit ni activer une option
simplement en la découvrant, ni accepter automatiquement des conditions au nom de
l'utilisateur. Une option non incluse ou désactivée doit être distinguée d'une panne.

## 7. État réel de la base 0.1.1

| Capacité | État dans le SDK actuel |
|---|---|
| Connecteurs intégrés sans dépendance ConnectorLab | Implémenté |
| Conversations Codex/Claude et MCP externe | Testés ; voir Validation.md |
| Modèle local à partir d'un moteur et d'un GGUF existants | Implémenté, inférence CPU testée |
| Scripts d'installation du compagnon et des plugins | Fournis |
| Activation optionnelle avec téléchargement conditionnel et messages de licence | Parcours commun à compléter ou à raccorder si cette option est incluse |
| État et installation des composants optionnels via IA | À exposer pour les fonctionnalités choisies |
| Utilisation automatique des outils par l'assistant embarqué | À raccorder et tester |
| Contexte métier et couverture des fonctions du projet cible | À adapter pour chaque intégration |

Les parcours manquants restent à réaliser pour les options retenues. Ils ne sont
pas obligatoires dans un plugin qui intègre uniquement le MCP et ne sont pas déjà
implémentés du seul fait de leur description dans ce document.

## 8. Recette de livraison

Vérifier d'abord que le plugin fonctionne sans l'option IA et sans ses composants.
Si elle est incluse, tester l'activation avec une installation existante puis avec
un composant manquant, les messages de licence, une acceptation réellement requise,
la connexion, le choix « Plus tard » et l'annulation. Vérifier la reprise et une
réponse du modèle après installation, sans téléchargement en double.
Tester les fonctions métier réutilisées via UI et MCP, une modification autorisée,
son refus sans permission et Undo quand annoncé. Vérifier les sorties, la fermeture
des sessions et FinishTask. Vérifier qu'aucun serveur ou panneau équivalent n'a été
dupliqué et que les fonctionnalités antérieures restent disponibles.

Livrer le compte rendu d'intégration, les instructions d'installation et les preuves
par version Unreal demandée entre 5.3 et 5.8. Distinguer compilation, exécution,
validation UI et limites restantes. Ne pas déclarer une intégration complète si le
parcours annoncé pour une option retenue ou l'accès aux fonctions métier manque encore.

## Sources officielles pour les parcours d'installation

Liens vérifiés le 21 septembre 2026 ; choisir et valider une version adaptée avant
installation. Les commandes d'installation ne sont pas exécutées par ce document.

- [Codex CLI — installation et connexion, documentation OpenAI](https://learn.chatgpt.com/docs/codex/cli).
- [Claude Code — installation, vérification et connexion](https://code.claude.com/docs/en/setup).
- [llama.cpp — versions publiées du moteur](https://github.com/ggml-org/llama.cpp/releases).
- Pour chaque modèle local, enregistrer dans le catalogue sa propre page officielle
  et sa licence ; ne pas substituer une page générique au modèle effectivement proposé.


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


## Required LICENCES folder in embedded plugins

Read [Licensing.md](Licensing.md). Every integration must ship the complete SDK MIT
notice at <Plugin>/LICENCES/CyUEMCPSDK-MIT.txt. Include Core/runtime component notices
when bundled, preserve the target plugin's own license, and verify LICENCES is present
in the delivered source/binary/Fab package, not only the checkout.
