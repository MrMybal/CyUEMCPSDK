# Licensing and redistribution

CyUEMCPSDK 0.1.8 is distributed under MIT in [LICENSE](../LICENSE).
This covers original C++ modules, assistant UI, Python companion, scripts and
documentation, including the CyUEMCP reference host. Incorporated third-party
components keep their own licenses and notices.

You may embed, modify and redistribute this SDK in commercial plugins without
licensing your own plugin code under MIT. Preserve the full MIT text and
Copyright (c) 2026 Cyberalien notice in copies or substantial portions of SDK code.
Include a copy of LICENSE in each embedded plugin distribution. These permissions
do not guarantee marketplace approval or replace Unreal/Fab terms.

The offline bundle contains Core 0.10.1 under MIT and companion/LICENSE.
Keep LICENSE-CyUEMCPSDK.txt, Licenses.txt and component license files inside the ZIP.
Python, dependencies, optional engines, providers and model weights keep their terms.

For existing integrations, see UpdateAssistantRuntime.md. Replace the ZIP and matching
manifest/notices together; verify the installed Core version and metadata.
Keep plugin identities, profiles and conversations isolated. The changed archive hash
selects another immutable runtime; do not relabel or overwrite an active pool.
Earlier copies distributed under GPL retain their granted rights.


## Required LICENCES folder in embedded plugins

Every integration must ship a LICENCES directory directly under the target plugin
root, beside Source, Resources and the .uplugin file:

- LICENCES/CyUEMCPSDK-MIT.txt: complete, unmodified MIT text from the SDK LICENSE,
  including Copyright (c) 2026 Cyberalien. Required even for MCP-only integrations.
- LICENCES/CyToolsCore-MIT.txt: complete Core LICENSE when Core is included.
- LICENCES/AssistantRuntime-Notices.txt: current component index from the bundled
  Licenses.txt when the assistant runtime is included. Preserve the full Python and
  third-party license files inside the archive as well.

Also include LICENCES/SOURCES.txt from the reference plugin: it links to both
GitHub repositories and their integration/creation instructions. Keep these links
when renaming the embedded integration.

These are component notices. Do not replace or relicense the target plugin's own
LICENSE, remove third-party attributions, or require the plugin itself to use MIT.
Include LICENCES in source/binary/Fab packages: adapt existing packaging rules,
RuntimeDependencies as NonUFS where needed, and Config/FilterPlugin.ini for
BuildPlugin. Inspect the actual delivered archive; a source-only folder is insufficient.
Follow the reference Build.ps1/Install.ps1 copies and FilterPlugin.ini rules.

## Source repositories

- CyToolsCore: https://github.com/MrMybal/CyToolsCore
- CyUEMCPSDK: https://github.com/MrMybal/CyUEMCPSDK

The repositories provide the source code and integration instructions. Preserve the
full license texts delivered with each component; repository links do not replace them.

Existing products must follow [UpdateExistingProducts.md](UpdateExistingProducts.md)
for the SDK/package migration and distribution notices.
