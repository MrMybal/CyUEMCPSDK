param([Parameter(Mandatory)][string]$Project,[ValidateSet('5.3','5.4','5.5','5.6','5.7','5.8')][string]$Version='5.8')
$ErrorActionPreference='Stop'
$Root=Split-Path $PSScriptRoot -Parent
$ProjectFile=Get-Item -LiteralPath $Project
if($ProjectFile.Extension -ne '.uproject'){throw 'Project must be an existing .uproject file'}
$Destination=Join-Path $ProjectFile.Directory.FullName 'Plugins'
foreach($Name in @('CyUEMCPSDK','CyUEMCP')){
 if(Test-Path -LiteralPath (Join-Path $Destination $Name)){throw "Plugin $Name already exists. Back it up and remove it explicitly before installing this version."}
 if(!(Test-Path -LiteralPath "$Root\Builds\UE$Version\Plugins\$Name\Binaries\Win64")){throw "Build UE $Version first with Scripts/Build.ps1"}
}
foreach($Name in @('CyUEMCPSDK','CyUEMCP')){
 $Target=Join-Path $Destination $Name
 New-Item -ItemType Directory -Force $Target | Out-Null
 Copy-Item -LiteralPath "$Root\Plugins\$Name\$Name.uplugin" -Destination $Target
 if(Test-Path "$Root\Plugins\$Name\Resources"){Copy-Item -LiteralPath "$Root\Plugins\$Name\Resources" -Destination $Target -Recurse}
 Copy-Item -LiteralPath "$Root\Plugins\$Name\Source" -Destination $Target -Recurse
 Copy-Item -LiteralPath "$Root\Builds\UE$Version\Plugins\$Name\Binaries" -Destination $Target -Recurse
}
Write-Output 'Enable CyUEMCP in Unreal Plugins, then restart. Open the assistant from its host integration; see Docs/AssistantContexts.md.'
Write-Output 'The assistant offers offline runtime setup at first opening. Shared user storage is the default; Settings > Runtime can install, check or repair it.'
