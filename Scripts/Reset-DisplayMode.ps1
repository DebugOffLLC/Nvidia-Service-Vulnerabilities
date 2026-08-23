<#
.SYNOPSIS
    Reinitializes the display to trigger shared memory creation.

.DESCRIPTION

.PARAMETER DeviceName
    Display device to flip, e.g. "\\.\DISPLAY1". Defaults to the primary display.

.PARAMETER DwellMs
    Milliseconds to hold the temporary resolution before restoring. Defaults to 750.

.EXAMPLE
    .\Reset-DisplayMode.ps1

.EXAMPLE
    .\Reset-DisplayMode.ps1 -DeviceName '\\.\DISPLAY1' -DwellMs 1500
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$DeviceName,

    [ValidateRange(0, 10000)]
    [int]$DwellMs = 750
)

if (-not ('DispMode' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DispMode {
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int EnumDisplaySettings(string deviceName, int modeNum, ref DEVMODE_RST devMode);
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int ChangeDisplaySettingsEx(string deviceName, ref DEVMODE_RST devMode, IntPtr hwnd, uint flags, IntPtr lParam);
}
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
public struct DEVMODE_RST {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
    public short dmSpecVersion;
    public short dmDriverVersion;
    public short dmSize;
    public short dmDriverExtra;
    public int   dmFields;
    public int   dmPositionX;
    public int   dmPositionY;
    public int   dmDisplayOrientation;
    public int   dmDisplayFixedOutput;
    public short dmColor;
    public short dmDuplex;
    public short dmYResolution;
    public short dmTTOption;
    public short dmCollate;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
    public short dmLogPixels;
    public int   dmBitsPerPel;
    public int   dmPelsWidth;
    public int   dmPelsHeight;
    public int   dmDisplayFlags;
    public int   dmDisplayFrequency;
    public int   dmICMMethod;
    public int   dmICMIntent;
    public int   dmMediaType;
    public int   dmDitherType;
    public int   dmReserved1;
    public int   dmReserved2;
    public int   dmPanningWidth;
    public int   dmPanningHeight;
}
'@
}

$ENUM_CURRENT_SETTINGS = -1
$DISP_CHANGE_SUCCESSFUL = 0

if ($PSBoundParameters.ContainsKey('DeviceName')) {
    $dev = $DeviceName
}
else {
    Add-Type -AssemblyName System.Windows.Forms
    $dev = [System.Windows.Forms.Screen]::PrimaryScreen.DeviceName
}

$DM_BITSPERPEL       = 0x00040000
$DM_PELSWIDTH        = 0x00080000
$DM_PELSHEIGHT       = 0x00100000
$DM_DISPLAYFREQUENCY = 0x00400000
$FIELDS = $DM_BITSPERPEL -bor $DM_PELSWIDTH -bor $DM_PELSHEIGHT -bor $DM_DISPLAYFREQUENCY

# Read current mode.
$orig = New-Object DEVMODE_RST
$orig.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($orig)
if ([DispMode]::EnumDisplaySettings($dev, $ENUM_CURRENT_SETTINGS, [ref]$orig) -eq 0) {
    Write-Error "EnumDisplaySettings failed for device '$dev'. Could not read current mode."
    return
}
$origW = $orig.dmPelsWidth; $origH = $orig.dmPelsHeight; $origBpp = $orig.dmBitsPerPel

# Find the largest supported resolution that differs from the current one.
$best = $null   # @{ W; H; Hz }
$i = 0
$probe = New-Object DEVMODE_RST
$probe.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($probe)
while ([DispMode]::EnumDisplaySettings($dev, $i, [ref]$probe) -ne 0) {
    if ($probe.dmBitsPerPel -eq $origBpp -and
        -not ($probe.dmPelsWidth -eq $origW -and $probe.dmPelsHeight -eq $origH)) {
        $area = $probe.dmPelsWidth * $probe.dmPelsHeight
        if ($null -eq $best -or $area -gt ($best.W * $best.H) -or
            ($area -eq ($best.W * $best.H) -and $probe.dmDisplayFrequency -gt $best.Hz)) {
            $best = @{ W = $probe.dmPelsWidth; H = $probe.dmPelsHeight; Hz = $probe.dmDisplayFrequency }
        }
    }
    $i++
}

if ($null -eq $best) {
    Write-Error "No alternate resolution available at ${origBpp}bpp on '$dev'; cannot force a distinct mode-set."
    return
}

$label   = $dev
$tmpDesc  = "$($best.W)x$($best.H) @ $($best.Hz)Hz"
$origDesc = "${origW}x${origH} @ $($orig.dmDisplayFrequency)Hz"

if (-not $PSCmdlet.ShouldProcess($label, "flip $origDesc -> $tmpDesc -> $origDesc (${DwellMs}ms dwell)")) {
    return
}

# Switch to the temporary resolution.
$tmp = New-Object DEVMODE_RST
$tmp.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($tmp)
$tmp.dmPelsWidth        = $best.W
$tmp.dmPelsHeight       = $best.H
$tmp.dmBitsPerPel       = $origBpp
$tmp.dmDisplayFrequency = $best.Hz
$tmp.dmFields           = $FIELDS

Write-Host "Flipping ${label}: $origDesc -> $tmpDesc ..." -ForegroundColor Cyan
$ret = [DispMode]::ChangeDisplaySettingsEx($dev, [ref]$tmp, [IntPtr]::Zero, 0, [IntPtr]::Zero)
if ($ret -ne $DISP_CHANGE_SUCCESSFUL) {
    Write-Error "Switch to $tmpDesc failed (ChangeDisplaySettingsEx returned $ret). Display left unchanged."
    return
}

Start-Sleep -Milliseconds $DwellMs

# Restore the original.
$orig.dmFields = $FIELDS
Write-Host "Restoring ${label}: $tmpDesc -> $origDesc ..." -ForegroundColor Cyan
$ret = [DispMode]::ChangeDisplaySettingsEx($dev, [ref]$orig, [IntPtr]::Zero, 0, [IntPtr]::Zero)
if ($ret -eq $DISP_CHANGE_SUCCESSFUL) {
    Write-Host "Display reinit complete; original mode restored." -ForegroundColor Green
}
else {
    Write-Error "Restore to $origDesc returned $ret. You may need to reset resolution manually."
}
