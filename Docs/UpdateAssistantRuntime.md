# Mettre à jour l'installation de l'assistant Unreal

Ce document est le point d'entrée pour une IA chargée de mettre à jour un plugin
qui contient déjà l'assistant. Référence : CyUEMCPSDK 0.1.8 ou version ultérieure.
Le dossier de ce document appartient au SDK de référence ; le plugin cible est
celui indiqué par l'utilisateur ou ouvert dans l'espace de travail.

## Avant de modifier le plugin

1. Lire les AGENTS.md applicables et examiner les modifications existantes.
2. Faire un commit local de sauvegarde du plugin avant la migration, après vérification,
   sans secrets ni caches. Sans Git, créer une sauvegarde datée et vérifiable.
3. Lire [UpdateExistingPlugin.md](UpdateExistingPlugin.md) et
   [AssistantRuntime.md](AssistantRuntime.md). Préserver les fonctionnalités,
   profils, permissions, conversations et adaptations déjà présentes.

## Migration demandée

Remplacer la livraison de Python et de ses dépendances sous forme de milliers de
fichiers par une archive ZIP hors ligne incluse dans le plugin.

- Embarquer le bundle de référence : Python portable, paquet officiel CyToolsCore,
  compagnon et dépendances exactes. Aucun modèle, CUDA, compte ou secret dans le ZIP.
- Inclure `Resources/AssistantRuntime/` : ZIP, `bundle.json`, `SetupRuntime.ps1`
  et `Licenses.txt`, avec leur déclaration NonUFS dans le module Editor existant.
  Ne pas livrer le venv du développeur ni une arborescence Python décompressée.
- À l'ouverture de l'assistant, vérifier en arrière-plan une installation compatible.
  Réutiliser celle qui est complète et valide. La vérification seule n'installe rien.
- Sinon, proposer **Install / repair**, **Later**, progression, annulation et licences.
  Extraire seulement après le clic explicite sur Install. Le reste du plugin doit
  rester utilisable lorsque l'assistant n'est pas installé.
- Utiliser le **dossier utilisateur partagé par défaut** :
  `%LOCALAPPDATA%/CyTools/UEAssistant/runtimes/`.
  Garder l'installation dans `<PLUGIN_ROOT>/Runtime/Assistant/` comme choix facultatif.
- Ajouter ou adapter **Settings > Runtime** pour vérifier, installer, réparer et choisir
  le stockage. Anglais par défaut. Conserver les chemins personnalisés dans Advanced.
- Lancer le Python vérifié du bundle ; aucun Python système ou chemin vers le dépôt
  de développement ne doit être nécessaire chez l'utilisateur.

## Réutilisation et isolation

Appliquer les contrôles et le verrouillage décrits dans AssistantRuntime.md : empreintes
complètes, versions/imports, chemins bornés, staging privé, publication après validation,
annulation et réparation sans écrasement d'une installation encore utilisée.

Seuls le code et les dépendances immuables sont mutualisés. Garder séparés les profils,
jetons, connexions, conversations, journaux, fichiers de tâche et processus des plugins.
Adapter les noms C++, ressources, services et espaces de configuration au plugin cible.
L'intégration reste dans ses modules existants : aucune dépendance Unreal externe à
CyUEMCPSDK, CyUEMCP ou CyAIConnectorLab et aucun nouveau module SDK séparé.

Réutiliser le même bundle lorsque son contenu convient. Si le compagnon ou les dépendances
nécessitent une adaptation, reconstruire le bundle avec `Scripts/PackageAssistantRuntime.py`
et son manifeste correspondant ; un contenu différent doit avoir une identité différente.
Ne jamais modifier une installation partagée pour y ajouter les adaptations d'un plugin.

## Sources à adapter

Dans le SDK de référence :

- `Plugins/CyUEMCPSDK/Source/CyUEAssistant/Private/CyAssistantRuntime.inl` : installation.
- `CyUEAssistant.cpp`, dans ce même dossier : démarrage, suivi et page Runtime.
- `CyAssistantNative.inl`, dans ce même dossier : chemins personnalisés.
- `Plugins/CyUEMCPSDK/Source/CyUEAssistant/CyUEAssistant.Build.cs` : ressources NonUFS.
- `Plugins/CyUEMCPSDK/Resources/AssistantRuntime/` : bundle et bootstrap.
- `Tests/test_runtime_installation.py` et test natif `CyUEMCP.Assistant.OfflineRuntimeSetup`.

Adapter ces éléments aux services existants ; ne pas remplacer aveuglément l'interface
ou le code métier du plugin.

## Livraison et vérification

Tester installation initiale, réutilisation depuis une autre copie/plugin, installation
locale, installations simultanées, annulation pendant l'extraction, archive endommagée,
réparation et démarrage réel du compagnon. Vérifier l'isolation de deux plugins intégrés
et l'absence de dépendance au dépôt du développeur ou à un Python préinstallé.

Valider l'interface sur UE 5.3 en premier, puis les versions demandées pour le plugin.
Ne pas déclarer testée une version seulement prévue. Mettre à jour sa documentation
et rendre un compte rendu bref : sauvegarde, changements, validations et limites.
Ne pas pousser ou publier automatiquement.

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
