param([string[]]$Versions=@('5.3','5.4','5.5','5.6','5.7','5.8'),[Parameter(Mandatory=$true)][string]$EngineRoot)
$ErrorActionPreference='Stop'
$Root=Split-Path $PSScriptRoot -Parent
foreach($Version in $Versions){
 $Build="$Root\Builds\UE$Version"
 New-Item -ItemType Directory -Force "$Build\Source\CyUEHost","$Build\Plugins","$Root\Reports" | Out-Null
 foreach($PluginName in @('CyUEMCPSDK','CyUEMCP')) {
  $PluginTarget=Join-Path "$Build\Plugins" $PluginName
  New-Item -ItemType Directory -Force $PluginTarget | Out-Null
  Copy-Item -LiteralPath "$Root\Plugins\$PluginName\$PluginName.uplugin" -Destination $PluginTarget -Force
  foreach($Folder in @('Source','Resources','Config','Content','LICENCES')) {
   $PluginSource=Join-Path "$Root\Plugins\$PluginName" $Folder
   if(Test-Path -LiteralPath $PluginSource) { Copy-Item -LiteralPath $PluginSource -Destination $PluginTarget -Recurse -Force }
  }
 }
 '{"FileVersion":3,"Modules":[{"Name":"CyUEHost","Type":"Runtime","LoadingPhase":"Default"}],"Plugins":[{"Name":"CyUEMCP","Enabled":true}]}' | Set-Content "$Build\CyUEHost.uproject" -Encoding utf8
 'using UnrealBuildTool; public class CyUEHost : ModuleRules { public CyUEHost(ReadOnlyTargetRules T):base(T){PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;PublicDependencyModuleNames.Add("Core");}}' | Set-Content "$Build\Source\CyUEHost\CyUEHost.Build.cs"
 '#include "Modules/ModuleManager.h"', 'IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, CyUEHost, "CyUEHost");' | Set-Content "$Build\Source\CyUEHost\CyUEHost.cpp"
 'using UnrealBuildTool; public class CyUEHostEditorTarget : TargetRules {public CyUEHostEditorTarget(TargetInfo T):base(T){Type=TargetType.Editor;DefaultBuildSettings=BuildSettingsVersion.Latest;ExtraModuleNames.Add("CyUEHost");}}' | Set-Content "$Build\Source\CyUEHostEditor.Target.cs"
 & "$EngineRoot\UE_$Version\Engine\Build\BatchFiles\Build.bat" CyUEHostEditor Win64 Development "$Build\CyUEHost.uproject" -WaitMutex -NoHotReloadFromIDE 2>&1 | Tee-Object "$Root\Reports\build-$Version.log"
 if($LASTEXITCODE){throw "UE $Version build failed"}
}
