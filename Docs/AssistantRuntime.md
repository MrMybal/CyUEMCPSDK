# Assistant runtime installation (0.1.7)

The Windows assistant ships one offline ZIP containing portable CPython 3.13.16,
CyToolsCore 0.10.0, the integrated companion and its exact dependencies. The ZIP
contains no model weights, GPU runtime, provider CLI, account or credential.
It works without Python installed on the computer. Linux/macOS bundles are not supplied.

## First opening

Opening the optional assistant starts a background check only. If a complete,
compatible installation exists, the assistant reuses it. Otherwise an English
setup window offers **Install / repair**, **Later**, and two storage choices:

- **Shared user folder**, selected by default:
  `%LOCALAPPDATA%/CyTools/UEAssistant/runtimes/win64-cp313-<first 32 SHA256 characters>`.
- **This plugin folder**: `<PLUGIN_ROOT>/Runtime/Assistant/win64-cp313-<first 32 SHA256 characters>`.
  This requires write access to the plugin folder.

Extraction starts only after clicking Install. The plugin remains usable without
its assistant. Closing the setup window or choosing Later installs nothing.
**Settings > Runtime** offers installation, checks, cancellation and component
notices later. Installation progress/errors are visible; the final result is also
written to connection logs. No network or administrator access is needed.
Existing custom Python/script overrides remain available in Providers > Advanced;
Save bridge / connect selects the custom runtime explicitly. Runtime settings apply
to the next companion start; they do not interrupt an existing conversation.

## Sharing and isolation

The entire archive hash identifies the immutable payload. The folder name uses its
first 32 characters to keep Windows paths short; verification checks the full hash. The same archive may
be used by several plugin installations and editors, even when the owning plugins
have different names. Changed Python ABI, dependencies or companion code produce
another hash/directory. Do not rename packages *inside the bundle* if their byte
identity should remain shareable. Native class/module/service names must still be
adapted to each embedded plugin's existing modules.

Only installed code/dependencies are shared. Keep provider profiles, authenticated
connection files, sessions, logs, upload files, task workspaces and local model
processes in the owning integration's separate storage. Do not put credentials,
conversations or models in the runtime pool. Python is launched with bytecode writes
disabled and a private `_pth` search path; user/global Python packages are not loaded.
This policy is specific to the lightweight Unreal assistant, not a new shared Python
policy for independent inference CyTools or their CUDA environments.

The bootstrap verifies the archive hash, inventory hash, every installed file,
exact package versions and imports before launching the companion. It rejects links,
path traversal, duplicate entries and declared-size overruns. Per-bundle OS file
locking coordinates simultaneous installs. Staging directories are private; only
a successfully validated runtime receives a completion marker and becomes visible.
Cancellation/failure removes that installer's staging directory, not another editor's
runtime. Repair publishes a new directory and updates a small pointer; it never
overwrites or deletes an existing installation another process may still be using.
Old versions and repaired directories are retained; automatic pool eviction is not
implemented. Remove unused versions only after the relevant editors have stopped.

## Packaging an embedded plugin

Read UpdateExistingPlugin.md first; commit a reviewed backup before migrating.
Include `Resources/AssistantRuntime/` as NonUFS resources: ZIP, `bundle.json`,
`SetupRuntime.ps1`, and `Licenses.txt`. Do not ship a developer venv, an extracted
Python tree or paths referring to the SDK checkout. The .NET/Windows PowerShell
bootstrap needs no external Python. Preserve first-use consent and Shared as default.
Adapt the native resource lookup to the target plugin and its private settings namespace.
Integrate the bootstrap into its existing Editor module and reuse an existing installer
if it can provide these checks. No separate CyUEMCPSDK plugin/module is required.

Run the standalone bootstrap only as an explicit user/install action:

```powershell
# Check only: never installs.
./Plugins/CyUEMCPSDK/Resources/AssistantRuntime/SetupRuntime.ps1 -Action Check -Report "$env:TEMP/cy-assistant-check.json"
# Explicit offline installation into the default shared pool.
./Plugins/CyUEMCPSDK/Resources/AssistantRuntime/SetupRuntime.ps1 -Action Install -Report "$env:TEMP/cy-assistant-install.json"
```

`-Mode Local -PluginRoot <path>` selects plugin-local storage. `-StorageRoot` is a
bootstrap override for controlled deployments/tests; the default native UI offers
Shared and Local only. `-CancelFile <path>` requests cooperative cancellation.
JSON states are Checking, Missing, Installing, Ready, Cancelled and Error. Ready
provides the verified Python and entry-point paths. Exit codes: 0 Check/Ready,
1 error, 2 cancelled. Do not treat exit 0 with Missing as an installed runtime.

## Building a new bundle

Maintainers use Scripts/PackageAssistantRuntime.py with the official verified Python
embeddable archive and the official built Core wheel. Python download/version/hash
are pinned in that script. It installs only runtime wheels into temporary staging,
retains component notices, strips machine-specific `direct_url.json`, tests imports,
and creates deterministic archive metadata. Exact resolved dependency versions are
recorded in both manifests. Rebuilding with updated dependency wheels changes the
content key; distribute the ZIP and its matching manifest together.

Scripts/Setup.ps1 is a development venv helper only. It is not an end-user installer.
Validate both first-install and reuse on a clean machine/account without Python;
check cancellation, concurrent installations, damage/repair, and isolation between
embedded plugins. The new native UI is validated on UE 5.3 first; do not claim newer
engine versions are validated by this change.
