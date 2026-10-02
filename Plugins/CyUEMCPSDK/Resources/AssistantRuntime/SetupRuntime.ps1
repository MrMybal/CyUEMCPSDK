[CmdletBinding()]
param(
    [ValidateSet('Check','Install')][string]$Action = 'Check',
    [ValidateSet('Shared','Local')][string]$Mode = 'Shared',
    [string]$PluginRoot,
    [string]$StorageRoot,
    [Parameter(Mandatory=$true)][string]$Report,
    [string]$CancelFile
)
# Windows PowerShell 5.1 / .NET bootstrap. No preinstalled Python or network is used.
$ErrorActionPreference = 'Stop'
if (-not $PluginRoot) { $PluginRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent }
$stage = $null
$lock = $null
$zip = $null
$destination = ''
function Publish-File([string]$Source, [string]$Target) {
    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    $backup = $Source + '.previous'
    while ($true) {
        try {
            if (Test-Path -LiteralPath $Target) { [IO.File]::Replace($Source, $Target, $backup) }
            else { [IO.File]::Move($Source, $Target) }
            if (Test-Path -LiteralPath $backup) { [IO.File]::Delete($backup) }
            return
        } catch {
            # A status reader may briefly hold a Windows handle without delete sharing.
            if (-not (Test-Path -LiteralPath $Source)) { return }
            if ($_.Exception.GetBaseException() -isnot [IO.IOException] -or [DateTime]::UtcNow -gt $deadline) { throw }
            Start-Sleep -Milliseconds 15
        }
    }
}
function Write-Report([string]$State, [string]$Message, [int]$Progress = 0, [bool]$Reused = $false) {
    $result = [ordered]@{state=$State; message=$Message; progress=$Progress; mode=$Mode; root=$destination; reused=$Reused}
    if ($State -eq 'Ready') {
        $result.python = Join-Path $destination $manifest.pythonExecutable
        $result.script = Join-Path $destination $manifest.entryPoint
        $result.bundle = $manifest.sha256
    }
    $parent = Split-Path -Parent $Report
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    $tempReport = $Report + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    [IO.File]::WriteAllText($tempReport, ($result | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    # Readers may briefly see no report; never a truncated JSON document.
    Publish-File $tempReport $Report
}
function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose(); $stream.Dispose() }
}
function Check-Cancel {
    if ($CancelFile -and (Test-Path -LiteralPath $CancelFile)) { throw [OperationCanceledException]::new('Installation cancelled') }
}
function Assert-NoReparse([string]$Path) {
    $itemPath = [IO.Path]::GetFullPath($Path)
    while ($itemPath) {
        if (Test-Path -LiteralPath $itemPath) {
            $item = Get-Item -LiteralPath $itemPath -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Runtime storage cannot contain links or junctions' }
        }
        $parent = Split-Path -Parent $itemPath
        if ($parent -eq $itemPath) { break }
        $itemPath = $parent
    }
}
function Bound-Path([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or $Relative.Contains(':') -or $Relative.Contains('\') -or [IO.Path]::IsPathRooted($Relative)) { throw 'Invalid archive path' }
    $parts = $Relative.Split('/')
    foreach ($part in $parts) {
        if ($part -eq '..' -or $part -eq '.' -or $part.EndsWith(' ') -or $part.EndsWith('.') -or $part -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') { throw 'Invalid archive path component' }
    }
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $path = [IO.Path]::GetFullPath((Join-Path $Root $Relative))
    if (-not $path.StartsWith($fullRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Archive entry escapes the installation' }
    return $path
}
function Validate-Runtime([string]$Root) {
    Assert-NoReparse $Root
    $inventory = Join-Path $Root 'runtime-manifest.json'
    if (-not (Test-Path -LiteralPath $inventory)) { throw 'Runtime inventory is missing' }
    if ((Get-Sha256 $inventory) -ne $manifest.runtimeManifestSha256) { throw 'Runtime inventory is damaged or incompatible' }
    $contents = Get-Content -LiteralPath $inventory -Raw | ConvertFrom-Json
    foreach ($file in $contents.files.PSObject.Properties) {
        Check-Cancel
        $path = Bound-Path $Root $file.Name
        Assert-NoReparse $path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw ('Missing runtime file: ' + $file.Name) }
        if ((Get-Sha256 $path) -ne $file.Value) { throw ('Damaged runtime file: ' + $file.Name) }
    }
    $python = Bound-Path $Root $manifest.pythonExecutable
    $verify = Bound-Path $Root 'verify-runtime.py'
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $python
    $info.Arguments = '-B "' + $verify + '"'
    $info.WorkingDirectory = $Root
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    try {
        if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Runtime validation timed out' }
        $output = $process.StandardOutput.ReadToEnd()
        $errorText = $process.StandardError.ReadToEnd()
        if ($process.ExitCode -ne 0) { throw ('Runtime dependency check failed: ' + $errorText.Substring([Math]::Max(0, $errorText.Length - 2000))) }
        $check = $output | ConvertFrom-Json
        if ($check.python -ne $manifest.pythonVersion -or $check.sdk -ne $manifest.sdkVersion) { throw 'Runtime version does not match this plugin' }
    } finally { $process.Dispose() }
}
function Find-Ready {
    $candidates = @($baseDestination)
    if (Test-Path -LiteralPath ($baseDestination + '.current.json')) {
        try {
            $pointer = Get-Content -LiteralPath ($baseDestination + '.current.json') -Raw | ConvertFrom-Json
            if ($pointer.name -match ('^' + [regex]::Escape($key) + '-repair-[a-f0-9]{16}$')) { $candidates = @((Join-Path $pool $pointer.name)) + $candidates }
        } catch { }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate '.complete.json')) {
            try {
                Validate-Runtime $candidate
                $script:destination = $candidate
                return $true
            } catch [OperationCanceledException] { throw } catch { Write-Verbose $_.Exception.Message }
        }
    }
    return $false
}
try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'bundle.json') -Raw | ConvertFrom-Json
    if ($manifest.formatVersion -ne 1 -or $manifest.platform -ne 'win64' -or $manifest.abi -ne 'cp313' -or $manifest.sha256 -notmatch '^[a-f0-9]{64}$' -or $manifest.runtimeManifestSha256 -notmatch '^[a-f0-9]{64}$') { throw 'Unsupported runtime manifest' }
    if (-not $StorageRoot) {
        if ($Mode -eq 'Shared') { $StorageRoot = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'CyTools\UEAssistant\runtimes' }
        else { $StorageRoot = Join-Path $PluginRoot 'Runtime\Assistant' }
    }
    $pool = [IO.Path]::GetFullPath($StorageRoot)
    Assert-NoReparse $pool
    $key = 'win64-cp313-' + $manifest.sha256.Substring(0, 32)
    $baseDestination = Join-Path $pool $key
    $destination = $baseDestination
    Write-Report 'Checking' 'Checking the assistant runtime'
    if (Find-Ready) { Write-Report 'Ready' 'Compatible assistant runtime found' 100 $true; exit 0 }
    if ($Action -eq 'Check') { Write-Report 'Missing' 'The assistant runtime needs installation or repair'; exit 0 }
    Check-Cancel
    [IO.Directory]::CreateDirectory($pool) | Out-Null
    Assert-NoReparse $pool
    Write-Report 'Installing' 'Waiting for the shared installation lock' 0
    $deadline = [DateTime]::UtcNow.AddMinutes(5)
    while (-not $lock) {
        Check-Cancel
        try { $lock = [IO.File]::Open((Join-Path $pool ($key + '.lock')), 'OpenOrCreate', 'ReadWrite', 'None') }
        catch [IO.IOException] { if ([DateTime]::UtcNow -gt $deadline) { throw 'Another runtime installation is still active. Try again later.' }; Start-Sleep -Milliseconds 150 }
    }
    if (Find-Ready) { Write-Report 'Ready' 'Reused the completed shared installation' 100 $true; exit 0 }
    Write-Report 'Installing' 'Verifying the offline archive' 2
    $archive = Bound-Path $PSScriptRoot $manifest.archive
    if ((Get-Sha256 $archive) -ne $manifest.sha256) { throw 'Offline archive checksum mismatch; restore the plugin package' }
    $stage = Join-Path $pool ('stage-' + [guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($stage) | Out-Null
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    $totalBytes = [long]0
    $names = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $zip.Entries) {
        Check-Cancel
        $path = Bound-Path $stage $entry.FullName
        if (-not $names.Add($path)) { throw 'Duplicate archive entry' }
        if (($entry.ExternalAttributes -shr 16 -band 0xF000) -eq 0xA000) { throw 'Archive links are not supported' }
        $totalBytes += $entry.Length
        if ($totalBytes -gt $manifest.extractedBytes -or $names.Count -gt $manifest.fileCount) { throw 'Archive exceeds its declared limits' }
    }
    if ($totalBytes -ne $manifest.extractedBytes -or $names.Count -ne $manifest.fileCount) { throw 'Archive size does not match its manifest' }
    $index = 0
    foreach ($entry in $zip.Entries) {
        Check-Cancel
        $path = Bound-Path $stage $entry.FullName
        [IO.Directory]::CreateDirectory((Split-Path -Parent $path)) | Out-Null
        $inputStream = $entry.Open()
        $outputStream = [IO.File]::Open($path, 'CreateNew', 'Write', 'None')
        try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose(); $inputStream.Dispose() }
        $index++
        if ($index % 50 -eq 0) { Write-Report 'Installing' 'Extracting Python and assistant dependencies' (5 + [int](70 * $index / $names.Count)) }
    }
    $zip.Dispose(); $zip = $null
    Write-Report 'Installing' 'Validating the installed runtime' 80
    Validate-Runtime $stage
    Check-Cancel
    # Never replace an existing directory: another editor can still be using it.
    $destination = $baseDestination
    if (Test-Path -LiteralPath $destination) { $destination = Join-Path $pool ($key + '-repair-' + [guid]::NewGuid().ToString('N').Substring(0,16)) }
    [IO.File]::WriteAllText((Join-Path $stage '.complete.json'), (@{sha256=$manifest.sha256; sdkVersion=$manifest.sdkVersion} | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
    Assert-NoReparse $pool
    [IO.Directory]::Move($stage, $destination)
    $stage = $null
    if ($destination -ne $baseDestination) {
        $pointer = $baseDestination + '.current.json'
        $pointerTemp = $pointer + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
        [IO.File]::WriteAllText($pointerTemp, (@{name=(Split-Path -Leaf $destination)} | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
        Publish-File $pointerTemp $pointer
    }
    Write-Report 'Ready' 'Assistant runtime installed' 100
    exit 0
} catch [OperationCanceledException] {
    Write-Report 'Cancelled' 'Installation cancelled; no partial runtime was activated'
    exit 2
} catch {
    Write-Report 'Error' $_.Exception.Message
    exit 1
} finally {
    if ($zip) { $zip.Dispose() }
    if ($stage -and (Test-Path -LiteralPath $stage)) {
        # Only this invocation's checked staging directory can be removed recursively.
        $absoluteStage = [IO.Path]::GetFullPath($stage)
        $absolutePool = [IO.Path]::GetFullPath($pool).TrimEnd('\') + '\'
        if ($absoluteStage.StartsWith($absolutePool, [StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $absoluteStage) -match '^stage-[a-f0-9]{32}$') {
            Assert-NoReparse $absoluteStage
            Remove-Item -LiteralPath $absoluteStage -Recurse -Force
        }
    }
    if ($lock) { $lock.Dispose() }
}
