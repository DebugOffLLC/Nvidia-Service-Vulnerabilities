#include "../Dump/include/SmartMax.h"

#pragma comment(lib, "user32.lib")

#define SHM_NAME     L"SMARTMAX_Shared_Memory"
#define TARGET_CLASS L"HideWindowTarget"
#define OFFSCREEN    (-10000L)

static constexpr DWORD MAX_MONRECTS = sizeof(((SMARTMAX_DISPLAY_ENTRY*)0)->cells) / sizeof(RECT);  // 32

int main()
{
    HWND hwnd = FindWindowW(TARGET_CLASS, nullptr);
    if (!hwnd) { printf("'%ls' not found\n", TARGET_CLASS); return 1; }

    HANDLE hMap = OpenFileMappingW(FILE_MAP_WRITE, FALSE, SHM_NAME);
    if (!hMap) { printf("OpenFileMappingW: %lu\n", GetLastError()); return 1; }

    BYTE* pShm = static_cast<BYTE*>(MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, sizeof(SMARTMAX_SHARED)));
    if (!pShm) { printf("MapViewOfFile: %lu\n", GetLastError()); CloseHandle(hMap); return 1; }
    SMARTMAX_SHARED* pSm = reinterpret_cast<SMARTMAX_SHARED*>(pShm);

    ULONGLONG state1 = pSm->TaggedState1;
    printf("Flags=0x%08lX  TaggedState1=0x%016llX\n", pSm->Flags, state1);
    if ((pSm->Flags & SMF_HOOK_GATE) != SMF_HOOK_GATE) printf("Hook gate not active\n");
    if (!(state1 & 1ULL))                               printf("Grid snap disabled\n");

    for (DWORD i = 0; i < ARRAYSIZE(pSm->Displays); i++) {
        SMARTMAX_DISPLAY_ENTRY& de = pSm->Displays[i];
        printf("    [%lu] Flags=%lu  rcDesktop={%ld,%ld,%ld,%ld}  '%ls'\n", i, de.Flags,
            de.rcDesktop.left, de.rcDesktop.top, de.rcDesktop.right, de.rcDesktop.bottom,
            (de.szDeviceName[0] ? de.szDeviceName : L"(empty)"));
    }

    ShowWindow(hwnd, SW_RESTORE);
    Sleep(150);

    RECT rcBefore = {};
    GetWindowRect(hwnd, &rcBefore);
    WINDOWPLACEMENT wp = { sizeof(wp) };
    GetWindowPlacement(hwnd, &wp);
    RECT rcNormal = wp.rcNormalPosition;
    printf("rcNormal={%ld,%ld,%ld,%ld}\n",
        rcNormal.left, rcNormal.top, rcNormal.right, rcNormal.bottom);

    MONITORINFOEXW miex = {};
    miex.cbSize = sizeof(miex);
    HMONITOR hMon = MonitorFromRect(&rcNormal, MONITOR_DEFAULTTONULL);
    if (!hMon) hMon = MonitorFromRect(&rcNormal, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(hMon, reinterpret_cast<LPMONITORINFO>(&miex));
    printf("Monitor: '%ls'  {%ld,%ld,%ld,%ld}\n", miex.szDevice,
        miex.rcMonitor.left, miex.rcMonitor.top, miex.rcMonitor.right, miex.rcMonitor.bottom);

    int targetEntry = -1;
    for (DWORD i = 0; i < ARRAYSIZE(pSm->Displays); i++) {
        if (lstrcmpW(miex.szDevice, pSm->Displays[i].szDeviceName) == 0) {
            targetEntry = static_cast<int>(i); break;
        }
    }
    if (targetEntry < 0) { printf("No match for '%ls', using [0]\n", miex.szDevice); targetEntry = 0; }
    printf("Display entry [%d]\n", targetEntry);

    SMARTMAX_DISPLAY_ENTRY& entry = pSm->Displays[targetEntry];
    DWORD  savedFlags = entry.Flags;
    DWORD  eFlags = entry.Flags;
    RECT   rcDesktop = entry.rcDesktop;
    RECT* pCells = entry.cells;
    DWORD  count = (eFlags < MAX_MONRECTS) ? eFlags : MAX_MONRECTS;

    WCHAR savedDevName[0x28] = {};
    memcpy(savedDevName, entry.szDeviceName, sizeof(savedDevName));
    if (lstrcmpW(entry.szDeviceName, miex.szDevice) != 0) {
        printf("szDeviceName '%ls' -> '%ls'\n",
            (entry.szDeviceName[0] ? entry.szDeviceName : L"(empty)"), miex.szDevice);
        wcsncpy_s(entry.szDeviceName, ARRAYSIZE(entry.szDeviceName), miex.szDevice, _TRUNCATE);
    }

    RECT savedCells[MAX_MONRECTS] = {};
    for (DWORD i = 0; i < count; i++) savedCells[i] = pCells[i];

    // Scale rcDesktop -> rcMonitor if Surround config differs from physical monitor size.
    float scaleX = 1.0f, scaleY = 1.0f;
    int deskW = rcDesktop.right - rcDesktop.left, deskH = rcDesktop.bottom - rcDesktop.top;
    int monW = (int)(miex.rcMonitor.right - miex.rcMonitor.left);
    int monH = (int)(miex.rcMonitor.bottom - miex.rcMonitor.top);
    bool scalingActive = (monW != deskW && monH != deskH && deskW != 0 && deskH != 0);
    if (scalingActive) {
        scaleX = (float)monW / (float)deskW;
        scaleY = (float)monH / (float)deskH;
        printf("Scaling: %.2fx%.2f\n", scaleX, scaleY);
    }

    // Pick the cell with the largest intersection with the window.
    int bestIdx = -1; LONG bestArea = 0;
    for (DWORD i = 0; i < count; i++) {
        RECT t = savedCells[i];
        if (scalingActive) {
            t.left = (LONG)((float)t.left * scaleX); t.top = (LONG)((float)t.top * scaleY);
            t.right = (LONG)((float)t.right * scaleX); t.bottom = (LONG)((float)t.bottom * scaleY);
        }
        RECT isect;
        if (IntersectRect(&isect, &rcNormal, &t)) {
            LONG area = (isect.right - isect.left) * (isect.bottom - isect.top);
            if (area > bestArea) { bestArea = area; bestIdx = (int)i; }
        }
    }
    if (bestIdx < 0) { printf("No cell intersects window, using [0]\n"); bestIdx = 0; }
    printf("cells[%d]  area=%ld\n", bestIdx, bestArea);

    // TaggedState1 bit 0 enables the grid-snap (cells) path in GetMaxToGridRect.
    pSm->TaggedState1 = state1 | 1ULL;
    // Inc Flags if the loop won't reach bestIdx (stale SHM after Surround reconfig).
    if (eFlags < (DWORD)(bestIdx + 1)) entry.Flags = (DWORD)(bestIdx + 1);

    // Top-left off-screen, far edges from rcMonitor (rcDesktop may be stale/zeroed).
    RECT newRect = { OFFSCREEN, OFFSCREEN, miex.rcMonitor.right, miex.rcMonitor.bottom };
    printf("New cells[%d]: {%ld,%ld,%ld,%ld}\n",
        bestIdx, newRect.left, newRect.top, newRect.right, newRect.bottom);
    for (DWORD i = 0; i < count; i++) { RECT z = {}; pCells[i] = z; }
    pCells[bestIdx] = newRect;

    BOOL triggered = ShowWindow(hwnd, SW_MAXIMIZE);
    if (!triggered) {
        printf("UIPI? Maximize the target manually, then press Enter...\n");
        (void)getchar();
    }
    else {
        Sleep(500);
    }

    RECT rcAfter = {};
    GetWindowRect(hwnd, &rcAfter);
    printf("Before: {%ld,%ld}  After: {%ld,%ld}\n", rcBefore.left, rcBefore.top, rcAfter.left, rcAfter.top);

    LONG expX = scalingActive ? (LONG)((float)OFFSCREEN * scaleX) : OFFSCREEN;
    LONG expY = scalingActive ? (LONG)((float)OFFSCREEN * scaleY) : OFFSCREEN;
    if (rcAfter.left == expX && rcAfter.top == expY) {
        printf("Window at (%ld,%ld).\n", rcAfter.left, rcAfter.top);
    }
    else if (rcAfter.left < -100 || rcAfter.top < -100) {
        printf("Off-screen: (%ld,%ld) (expected ~(%ld,%ld)).\n", rcAfter.left, rcAfter.top, expX, expY);
    }
    else if (rcAfter.left != rcBefore.left || rcAfter.top != rcBefore.top) {
        printf("Moved to (%ld,%ld) but not fully off-screen.\n", rcAfter.left, rcAfter.top);
    }
    else {
        printf("Did not move. '%ls' in SHM?\n", miex.szDevice);
    }

    printf("Press enter to restore...\n");
    (void)getchar();

    DWORD restoreCount = max(count, (DWORD)(bestIdx + 1));
    for (DWORD i = 0; i < restoreCount; i++) pCells[i] = savedCells[i];
    entry.Flags = savedFlags;
    pSm->TaggedState1 = state1;
    memcpy(entry.szDeviceName, savedDevName, sizeof(savedDevName));

    ShowWindow(hwnd, SW_RESTORE);
    SetWindowPos(hwnd, nullptr, 100, 100, 480, 200, SWP_NOZORDER | SWP_NOACTIVATE);
    printf("Restored.\n");

    UnmapViewOfFile(pShm);
    CloseHandle(hMap);
    return 0;
}