[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Position = 0)]
    [string]$Name,

    [string]$TriggerScript = (Join-Path $PSScriptRoot 'Toggle-RefreshRate.ps1'),

    [ValidateRange(1, 120)]
    [int]$TimeoutSeconds = 8,

    [switch]$NoTrigger,

    [switch]$CheckHolders
)

Import-Module NtObjectManager -ErrorAction Stop

if (-not $PSBoundParameters.ContainsKey('Name')) {
    $session = (Get-NtProcess -Current).SessionId
    $Name = "\Sessions\$session\BaseNamedObjects\SMARTMAX_Shared_Memory"
}
$Path = $Name

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

if (-not (Test-LeafPresent -Parent $ParentPath -LeafName $Leaf)) {
    Write-Host "'$Path' is already free. Skip the crash -- run Squat-NamedObject.ps1 now." -ForegroundColor Yellow
    return
}

if ($CheckHolders) {
    try {
        $holders = Get-NtHandle -ObjectType Section -ErrorAction Stop |
            Where-Object { $_.Name -and $_.Name.EndsWith($Leaf) }
        $byPid = $holders | Group-Object ProcessId
        Write-Host ("[holders] {0} handle(s) across {1} process(es):" -f $holders.Count, $byPid.Count) -ForegroundColor Cyan
        foreach ($g in $byPid) {
            $pname = (Get-Process -Id $g.Name -ErrorAction SilentlyContinue).ProcessName
            Write-Host ("  PID {0,-6} {1}  ({2} handle(s))" -f $g.Name, $pname, $g.Count)
        }
        if ($byPid.Count -gt 4) {
            Write-Warning "More than 4 holders -- Surround is likely injected widely. Freeing the name will take many faults and may exceed the 10/300 watchdog budget. Consider logoff/reboot instead."
        }
    }
    catch {
        Write-Warning "Holder enumeration unavailable (need elevation/SeDebugPrivilege): $($_.Exception.Message)"
    }
}

if (-not $PSCmdlet.ShouldProcess($Path, "memset view to 0xFF (single poison)")) {
    return
}

try {
    $section = Get-NtSection $Path -Access MapRead, MapWrite -ErrorAction Stop
}
catch [NtCoreLib.NtException] {
    switch ($_.Exception.Status) {
        'STATUS_ACCESS_DENIED'         { Write-Error "Access denied opening '$Path' for write. Try an elevated session." }
        'STATUS_OBJECT_TYPE_MISMATCH'  { Write-Error "'$Path' exists but is not a Section object; refusing to continue." }
        'STATUS_OBJECT_NAME_NOT_FOUND' { Write-Host "'$Path' vanished before it could be opened; already gone." -ForegroundColor Yellow }
        default                        { Write-Error "Failed to open '$Path': $($_.Exception.Message)" }
    }
    return
}

$map = $null
try {
    $map = $section.MapReadWrite()
    $map.FillBuffer(0xFF)
    Write-Host "[poison] '$Path' view filled with 0xFF ($($map.Length) bytes)." -ForegroundColor Green
}
catch [NtCoreLib.NtException] {
    Write-Error "Mapping/writing '$Path' failed: $($_.Exception.Message)"
    return
}
finally {
    if ($map) { $map.Dispose() }
    $section.Close()
}

if ($NoTrigger) {
    Write-Host "[trigger] -NoTrigger set; poison is in place. Fire ONE display change yourself to free the name." -ForegroundColor Yellow
}
elseif (-not (Test-Path $TriggerScript)) {
    Write-Warning "Trigger script not found: $TriggerScript. Poison is in place -- fire ONE display change yourself."
}
elseif ($PSCmdlet.ShouldProcess($TriggerScript, "launch single display-change trigger (detached)")) {
    Start-Process -FilePath 'powershell.exe' `
        -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$TriggerScript`"" `
        -WindowStyle Hidden | Out-Null
    Write-Host "[trigger] launched once: $TriggerScript" -ForegroundColor Green
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$freed = $false
while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
    if (-not (Test-LeafPresent -Parent $ParentPath -LeafName $Leaf)) { $freed = $true; break }
    Start-Sleep -Milliseconds 200
}
$sw.Stop()

if ($freed) {
    Write-Host ("[free] '{0}' released after {1}s. It will stay free until the next SmartMax trigger (lazy recreation)." -f $Path, [math]::Round($sw.Elapsed.TotalSeconds, 2)) -ForegroundColor Green
    Write-Host "       -> If Squat-NamedObject.ps1 is already waiting it has planted the link. Otherwise run it now, then Toggle-RefreshRate.ps1." -ForegroundColor Green
}
else {
    Write-Warning "'$Path' is still present after ${TimeoutSeconds}s -- not all holders faulted from one trigger."
    Write-Warning "Do NOT spam this (that is the storm that exhausts the watchdog). Re-run ONCE, or evict deterministically with logoff/reboot."
}
