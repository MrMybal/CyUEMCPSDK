# Mettre à jour un plugin Unreal déjà intégré

Ce guide concerne une migration, pas une première intégration. Référence actuelle :
CyUEMCPSDK 0.1.8. Lire la version réellement présente dans README.md et les fichiers
.uplugin au moment de l'intervention. Adapter au plugin cible ; ne pas repartir de zéro.

## 1. Sauvegarder avant toute modification

- Lire les AGENTS.md applicables et examiner git status / les différences existantes.
- Faire un commit local de sauvegarde de l'état du plugin avant la migration. Inclure
  ses modifications existantes pertinentes après vérification ; ne pas ajouter de
  secrets, comptes, poids, caches, environnements Python ou binaires générés.
- Ne pas utiliser git reset/clean, écraser le travail existant ou publier/pousser le
  commit automatiquement. Laisser les changements étrangers au plugin intacts.
- Si aucun dépôt n'existe, faire une sauvegarde datée vérifiable du périmètre cible
  et signaler cette exception. Ne pas initialiser un dépôt dans un dossier parent large.
- Consigner le hash du commit ou le chemin de sauvegarde dans le rapport de migration.

## 2. Lire les conventions et auditer l'intégration existante

Sources de référence : <SDK_ROOT>. Socle : <CORE_ROOT>.
Ces chemins servent au développement local, jamais de chemins requis chez le client.
Lire README.md, Docs/Integration.md, Docs/ProjectIntegration.md, Docs/Architecture.md,
Docs/AssistantUI.md, Docs/Providers.md, Docs/ConnectorConventions.md et Docs/Validation.md.
Pour la migration de l’interface et du multi-instance, appliquer aussi
[UpdateAssistantInterface.md](UpdateAssistantInterface.md) et [AssistantContexts.md](AssistantContexts.md).
Pour les opérations et artefacts, lire aussi les guides CyToolsCore AutomaticInputs,
TaskFiles et FullOperationAccess. Les instructions du projet cible restent applicables.

Repérer dans le plugin : version intégrée, modules, dépendances, services métier,
registre d'opérations, serveur MCP, assistant, transports, profils, permissions,
installateurs, stockage et éventuelles adaptations du SDK. Comparer avec la référence.
Créer ou compléter Docs/CyUEMCPIntegration.md avec la matrice :
fonction existante / point d'entrée / contrat IA / UI / adaptation / preuve de test.

Réutiliser les fonctions métier, connecteurs, assistants et installateurs déjà présents.
Ne pas retirer une capacité spécifique au projet au motif qu'elle n'existe pas dans
l'exemple. Préserver les opérations publiques, paramètres, sorties et réglages des
utilisateurs ; fournir une migration explicite si un contrat doit réellement changer.

## 3. Intégration incluse dans le plugin cible — obligatoire

Livrer un seul plugin : celui du projet. L'assistant, le MCP, les connecteurs et leurs
ressources font partie de ce plugin, avec son .uplugin existant. Aucun plugin ou module
SDK séparé à installer, activer ou livrer à côté. Le dossier CyUEMCPSDK est une référence
de sources et de conventions, pas une dépendance Unreal externe imposée au client.
Cette règle remplace l'ancien montage par plugins CyUEMCPSDK + CyUEMCP séparés.

- Intégrer le code adapté dans les modules existants du plugin, en particulier son
  module Editor. Ne pas créer un nouveau module SDK séparé pour cette migration.
  Garder les dépendances Editor hors des modules runtime métier.
- Réutiliser l'intégration existante et comparer ses adaptations avant remplacement.
  Inclure les ressources de chat, données de providers et composants bridge nécessaires
  dans le package du plugin. Aucun chemin vers le dépôt de développement chez le client.
- Ne pas ajouter de dépendance .uplugin à CyUEMCPSDK ou CyUEMCP. Si elle existe déjà,
  intégrer ses fonctionnalités avant de retirer cette dépendance et tester sans ces
  plugins externes. Ne pas perdre de fonctions lors de ce changement.
- Préserver les contrats et schémas CyToolsCore. Inclure ou installer le paquet Core
  officiel et les dépendances nécessaires via le plugin, avec versions et notices ;
  ne pas en créer une réimplémentation divergente ni embarquer le venv du développeur.
  Aucun ConnectorLab, CyMD ou CyAIOrchestrator externe requis.
- Adapter les noms de classes, symboles, ressources et points d'entrée au plugin cible
  pour éviter les collisions. Préserver les IDs d'opérations déjà publics.
- Plusieurs plugins intégrés doivent pouvoir cohabiter sans double enregistrement,
  conflit de port ou dépendance cachée à l'installation d'un autre plugin. Réutiliser
  un service commun déjà présent seulement s'il est disponible, avec un fonctionnement
  autonome du plugin en son absence. Tester les deux cas.
- Scripts/Install.ps1 installe les plugins d'exemple séparés : ne pas l'utiliser pour
  cette livraison intégrée. Réaliser la migration dans le plugin cible, sauvegardée
  et réversible, puis valider son package seul dans un projet propre.

## 4. Adapter l'interface de l'assistant existant

Ne pas ajouter d’entrée globale **Assistant** ni **New assistant window** dans le
menu Window par défaut. L’ouverture appartient à l’interface du plugin ou du toolkit
hôte, avec son contexte. Retirer les anciennes entrées ajoutées par l’intégration,
sans supprimer les menus d’un autre plugin. Préserver les fenêtres métier légitimes.

- Une seule barre compacte : conversations à gauche, engrenage Settings à droite.
  Supprimer la rangée supplémentaire Chat / Settings. Les réglages restent dans
  chaque instance ; pas de fenêtres séparées Connections, Local Model ou Logs.
  Afficher la version réelle de l’assistant en petit en bas.
- Onglets natifs façon Chrome : formes arrondies, état actif lisible, croix intégrée,
  bouton + discret, plusieurs conversations indépendantes. Pas une rangée de boutons
  standard pour simuler des onglets. Le clic central peut fermer une conversation.
- Settings contient **Providers** et **Connection logs**. Providers regroupe les
  connexions CLI/API et les modèles locaux ; le moteur local se configure dans son
  profil. Chaque profil possède Connection, Permissions et Advanced.
- Anglais par défaut. Préserver les traductions déjà disponibles et ne pas mélanger
  arbitrairement les langues. Les options rares vont dans Advanced.
- Afficher le connecteur, son transport réel, le modèle utilisé et les droits effectifs.
  Ne pas appeler « SDK » un transport CLI ni afficher un modèle inventé.
- Conserver Markdown, sélection/copie du texte, pièces jointes, collage/glisser-déposer
  et aperçus compatibles. Pas de bouton Copy message permanent sous chaque réponse.
- Préserver le streaming fluide et la lecture de l'historique sans défilement forcé.
  Changer de page ou d'onglet ne perd ni message, ni brouillon, ni génération en cours.
- Chaque conversation possède sa session et son contexte. Fermer l'une ne ferme pas
  les autres. Journaux consultables, sélectionnables, copiables et exportables.

## 5. Providers, connexions et droits

Réutiliser les données Resources/Connectors et les transports intégrés. La référence
comprend Codex App Server, Codex CLI, Claude CLI, Antigravity CLI, OpenCode CLI,
Ollama, Claude API, OpenRouter API et local llama.cpp. Préserver les autres providers
réellement fonctionnels du plugin cible ; ne pas les remplacer par un catalogue réduit.

Profils multiples persistants, presets par défaut sans écraser les profils existants,
découverte automatique, contrôle au lancement et Test connection manuel. La découverte
ne génère pas de réponse et ne télécharge rien implicitement. Signaler précisément une
installation manquante, une authentification non vérifiée ou une clé absente.

Les API gardent l'URL et le nom de variable d'environnement dans le profil, jamais la
clé. Préserver un coffre sécurisé existant s'il est déjà adapté. Pas de secrets dans
les logs, exports ou commits. Les modèles locaux appartiennent aux Providers ; ne
pas copier les poids dans les profils ni dans les sauvegardes de configuration.

Les droits sont réellement appliqués par le backend et figés pour chaque conversation.
Une modification du profil s'applique aux nouvelles conversations. Distinguer les droits
fichiers du provider et l'autorisation des mutations Unreal via MCP.

MCP est un accès aux outils, pas automatiquement un provider de conversation. Conserver
ou raccorder les appels d'outils de l'assistant lorsque le plugin les fournit déjà ou
les annonce : tester une vraie opération. La référence ne câble pas automatiquement
les outils Unreal dans chaque provider. Ne pas livrer un simple chat en prétendant
qu'il contrôle le plugin, ni supprimer un câblage opérationnel pendant la migration.

## 6. Conserver les garanties métier et fichiers

Exposer les capacités réelles complètes et leurs options aux clients IA. Les formulaires
simples ne doivent pas limiter le contrat. Identités issues du transport authentifié,
validation native, mutations sur le bon thread, transactions/Undo et sauvegarde explicite.

Accepter les chemins absolus autorisés via les imports privés automatiques du Core.
Déclarer tous les champs de fichiers ; ne jamais exiger une copie manuelle de l'IA.
Sessions IA temporaires : exporter les résultats souhaités avant FinishTask, nettoyer
copies importées et workspaces après l'arrêt réel des workers. Préserver les originaux,
poids et assets effectivement intégrés au projet Unreal. Ne pas relâcher une réservation
alors qu'un worker est encore actif, même après annulation ou délai HTTP dépassé.

## 7. Livraison visée : plugins commerciaux sur Fab

Le plugin métier reste utilisable sans compte IA ni modèle local. L'assistant et les
moteurs optionnels doivent réutiliser une installation présente ou proposer une activation
compréhensible avec provenance, licence, connexion/acceptation et téléchargement seulement
si nécessaire. Implémenter ou raccorder ce parcours ; sa description n'est pas une preuve.

Préparer les sources/modules et les ressources nécessaires au package. Définir comment
le Core, le bridge et le runtime requis sont distribués ou installés sans dépendre du
poste de développement. Tester cette stratégie dans un projet propre. Ne pas livrer
le venv de développement ni ses chemins absolus comme solution d'installation.

Inventorier les composants réellement distribués et joindre leurs notices/licences.
Ne pas redistribuer automatiquement les CLI tiers, comptes ou poids. Ne pas imposer
une nouvelle licence au code propriétaire du projet. Vérifier les exigences Fab en
vigueur lors de la préparation du package ; ne pas annoncer une approbation Fab obtenue.

## 8. Valider et livrer la migration

- Pour cette évolution UI 0.1.7, commencer par UE 5.3, sauf autre demande explicite.
  Après validation de l’interface, compiler les autres versions demandées. Ne pas annoncer
  compatible une version seulement supposée ; préciser tout moteur indisponible.
- Tester en contexte natif : instances contextuelles indépendantes, Providers/local/logs, création/modification
  des profils, échec puis retest de connexion, vraie réponse d'un provider disponible,
  affichage du modèle, deux conversations indépendantes et conservation après fermeture.
- Vérifier les droits lecture/écriture dans un périmètre de test, les opérations MCP
  métier, les entrées externes, exports et nettoyage. Protéger les assets utilisateur.
- Tester l'installation propre et l'absence de collision avec les plugins partageant
  ce socle. Ne pas arrêter au seul build, à un mock ou à une capture d'écran.
- Si un compte/CLI/modèle manque, terminer les parties indépendantes et indiquer le
  test bloqué sans inventer un succès ni supprimer le provider concerné.
- Livrer le code adapté, la migration des réglages, les instructions utilisateur,
  Docs/CyUEMCPIntegration.md à jour, captures natives et preuves/limites des tests.
  Fournir le commit de sauvegarde, les fichiers modifiés et les étapes de mise à jour.


## Offline assistant setup (0.1.8)

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

For the coordinated SDK/version/license update, begin with [UpdateExistingProducts.md](UpdateExistingProducts.md).
