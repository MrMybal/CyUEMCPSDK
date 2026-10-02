# Mettre à jour les plugins et logiciels utilisant les SDK Cy

Point d'entrée commun :
[CyToolsCore — UpdateExistingProducts.md](https://github.com/MrMybal/CyToolsCore/blob/main/Docs/UpdateExistingProducts.md).

Si les sources sont présentes localement, lire le même fichier depuis
<CORE_ROOT>/Docs/UpdateExistingProducts.md. Les dépôts SDK servent de référence au
développement ; ils ne doivent pas devenir des chemins requis chez le client.

## Migration d'un plugin Unreal existant

1. Appliquer le guide commun, commencer par un commit de sauvegarde du plugin.
2. Lire [UpdateExistingPlugin.md](UpdateExistingPlugin.md),
   [UpdateAssistantRuntime.md](UpdateAssistantRuntime.md) et [Licensing.md](Licensing.md).
3. Adapter dans les modules existants du plugin. Préserver ses fonctions métier,
   configurations et adaptations ; vérifier les noms et l'isolation de chaque intégration.
4. Utiliser les versions MIT vérifiées : Core 0.10.1 et CyUEMCPSDK 0.1.8, ou des versions
   ultérieures dont licence et compatibilité ont réellement été vérifiées.
5. Remplacer les vrais paquets/ZIP sélectionnés par le produit, leurs manifestes et
   notices ; reconstruire les livrables concernés. Ne pas seulement changer un label.
6. Distribuer le dossier racine LICENCES avec les textes MIT complets, SOURCES.txt,
   les liens GitHub et les notices tierces applicables. Préserver la licence du plugin.
7. Valider le produit final, sa coexistence avec d'autres plugins et les versions Unreal
   réellement exécutées. Une migration d'un plugin ne valide pas tous les autres.

Un logiciel utilisant seulement le Core suit la branche correspondante du guide commun.
L'assistant et l'inférence locale restent optionnels ; cette migration n'ajoute pas
automatiquement ces fonctionnalités à un projet qui ne les possède pas.
Ne pas pousser/publier les produits cibles sans autorisation couvrant cela.
