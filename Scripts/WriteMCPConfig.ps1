param([Parameter(Mandatory)][string]$EditorConnection,[Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
$Root=Split-Path $PSScriptRoot -Parent
$Connection=(Get-Item -LiteralPath $EditorConnection).FullName
$Configuration=@{mcpServers=@{CyUEMCP=@{command="$Root\Runtime\python\Scripts\python.exe";args=@("$Root\Bridge\run.py",'--editor',$Connection,'--mcp')}}}
$Configuration | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $Output -Encoding utf8
Write-Output "Configuration written: $Output (contains no token)"
