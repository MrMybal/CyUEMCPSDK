param([string]$CoreRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) '../CyToolsCore'),[string]$Python = 'python')
$ErrorActionPreference='Stop'
$Root=Split-Path $PSScriptRoot -Parent
& $Python -m venv "$Root\Runtime\python"
if($LASTEXITCODE){throw 'venv creation failed'}
$Venv="$Root\Runtime\python\Scripts\python.exe"
& $Venv -m pip install "$CoreRoot[mcp]" "$Root\Bridge"
if($LASTEXITCODE){throw 'dependency installation failed'}
Write-Output "Python executable: $Venv"
Write-Output "Companion script: $Root\Bridge\run.py"
