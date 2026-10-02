param([Parameter(Mandatory)][string]$EditorConnection,[Parameter(Mandatory)][string]$Output,[ValidateSet('Shared','Local')][string]$Mode='Shared',[string]$PluginRoot)
$ErrorActionPreference='Stop'
$Root=Split-Path $PSScriptRoot -Parent
if (-not $PluginRoot) { $PluginRoot=Join-Path $Root 'Plugins/CyUEMCPSDK' }
$Connection=(Get-Item -LiteralPath $EditorConnection).FullName
$Report=Join-Path ([IO.Path]::GetTempPath()) ('cyue-mcp-runtime-'+[guid]::NewGuid().ToString('N')+'.json')
try {
 & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PluginRoot\Resources\AssistantRuntime\SetupRuntime.ps1" -Action Check -Mode $Mode -PluginRoot $PluginRoot -Report $Report
 if ($LASTEXITCODE -ne 0) { throw 'Runtime check failed; use Settings > Runtime for diagnostics.' }
 $Runtime=Get-Content -LiteralPath $Report -Raw | ConvertFrom-Json
 if ($Runtime.state -ne 'Ready') { throw 'Install the assistant runtime explicitly from Settings > Runtime first.' }
 $Configuration=@{mcpServers=@{CyUEMCP=@{command=$Runtime.python;args=@('-B',$Runtime.script,'--editor',$Connection,'--mcp')}}}
 $Configuration | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Output -Encoding utf8
 Write-Output "Configuration written: $Output (contains no token)"
} finally { if (Test-Path -LiteralPath $Report) { Remove-Item -LiteralPath $Report -Force } }
