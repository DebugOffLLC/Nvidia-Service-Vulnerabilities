#include "../include/SmartMax.h"

/// <summary>
/// Print a 16-byte-wide hex dump with ASCII sidebar.
/// </summary>
/// <param name="pData">Buffer to dump.</param>
/// <param name="stLen">Byte count to dump.</param>
/// <param name="dwBaseOffset">Offset label for the first row.</param>
static VOID HexDump(
    _In_reads_bytes_(stLen) const BYTE* pData,
    _In_ size_t                         stLen,
    _In_ DWORD                          dwBaseOffset);

/// <summary>
/// Read and print all SMARTMAX_SHARED regions from a read-only SHM view.
/// </summary>
/// <param name="pShm">Pointer to the mapped SMARTMAX_SHARED (read-only).</param>
static VOID PrintSmartmaxShm(
    _In_ const SMARTMAX_SHARED* pShm);

int wmain(void)
{
    HANDLE           hMap = NULL;
    SMARTMAX_SHARED* pShm = NULL;
    int              nRet = 1;

    hMap = OpenFileMappingW(FILE_MAP_WRITE, FALSE, L"SMARTMAX_Shared_Memory");
    if (!hMap)
    {
        wprintf(L"[-] OpenFileMappingW failed (%lu).\n"
            L"    Ensure NVIDIA Surround is enabled.\n",
            GetLastError());
        goto __done;
    }

    pShm = static_cast<SMARTMAX_SHARED*>(
        MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, sizeof(SMARTMAX_SHARED)));
    if (!pShm)
    {
        wprintf(L"[-] MapViewOfFile failed (%lu).\n", GetLastError());
        goto __done;
    }

    wprintf(L"[+] SMARTMAX_Shared_Memory  base=%p  size=0x%zX (%zu bytes)\n\n",
        static_cast<void*>(pShm), sizeof(SMARTMAX_SHARED), sizeof(SMARTMAX_SHARED));

    PrintSmartmaxShm(pShm);

    wprintf(L"=== Raw hex dump ===\n");
    wprintf(L"OFFSET    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  |0123456789ABCDEF|\n");
    wprintf(L"--------  -----------------------------------------------  ------------------\n");
    HexDump(reinterpret_cast<const BYTE*>(pShm), sizeof(SMARTMAX_SHARED), 0);

    nRet = 0;

__done:
    if (pShm) UnmapViewOfFile(pShm);
    if (hMap) CloseHandle(hMap);
    return nRet;
}

static VOID PrintSmartmaxShm(
    _In_ const SMARTMAX_SHARED* pShm)
{
    if (!pShm) return;

    // Header
    {
        DWORD     dwInitMask = pShm->InitMask;
        DWORD     dwFlags = pShm->Flags;
        UINT64    ui64TrackedFG = reinterpret_cast<UINT64>(pShm->hwndTrackedFG);
        UINT64    ui64App = reinterpret_cast<UINT64>(pShm->hwndApp);
        DWORD     dwSetHookArr = pShm->SetHookArrivals;
        DWORD     dwUnHookArr = pShm->UnHookArrivals;
        DWORD     dwSetHookInFl = pShm->SetHookInFlight;
        DWORD     dwUnHookInFl = pShm->UnHookInFlight;
        ULONGLONG ui64TS1 = pShm->TaggedState1;
        ULONGLONG ui64TS2 = pShm->TaggedState2;
        DWORD     dwTS3 = pShm->TaggedState3;
        DWORD     dwS4 = pShm->State4;
        DWORD     dwMovingTaskbar = pShm->MovingTaskbar;

        wprintf(L"=== SMARTMAX_SHARED header  (SHM+0x0000) ===\n");
        wprintf(L"  InitMask          0x%08lX\n", dwInitMask);

        wprintf(L"  Flags             0x%08lX  [", dwFlags);
        if (dwFlags & SMF_ENABLED)       wprintf(L" SMF_ENABLED");
        if (dwFlags & SMF_HOOKS_ACTIVE)  wprintf(L" SMF_HOOKS_ACTIVE");
        if (dwFlags & SMF_DISPLAY_READY) wprintf(L" SMF_DISPLAY_READY");
        if (dwFlags & SMF_PHASE_INIT)    wprintf(L" SMF_PHASE_INIT");
        if (dwFlags & SMF_PHASE_HOOKS)   wprintf(L" SMF_PHASE_HOOKS");
        if (dwFlags & SMF_PHASE_READY)   wprintf(L" SMF_PHASE_READY");
        wprintf(L" ]%s\n",
            (dwFlags & SMF_HOOK_GATE) == SMF_HOOK_GATE
            ? L"  (hook gate SATISFIED)" : L"  (hook gate NOT met)");

        wprintf(L"  hwndTrackedFG     0x%016llX\n", ui64TrackedFG);
        wprintf(L"  hwndApp           0x%016llX\n", ui64App);
        wprintf(L"  SetHookArrivals   %lu\n", dwSetHookArr);
        wprintf(L"  UnHookArrivals    %lu\n", dwUnHookArr);
        wprintf(L"  SetHookInFlight   %lu\n", dwSetHookInFl);
        wprintf(L"  UnHookInFlight    %lu\n", dwUnHookInFl);
        wprintf(L"  TaggedState1      0x%016llX\n", ui64TS1);
        wprintf(L"  TaggedState2      0x%016llX\n", ui64TS2);
        wprintf(L"  TaggedState3      0x%08lX\n", dwTS3);
        wprintf(L"  State4            0x%08lX\n", dwS4);
        wprintf(L"  MovingTaskbar     0x%08lX\n\n", dwMovingTaskbar);
    }

    // Process table
    {
        wprintf(L"=== Process table  SHM+0x%04zX  (%zu entries x 0x%02zX bytes) ===\n",
            static_cast<size_t>(FIELD_OFFSET(SMARTMAX_SHARED, Apps)),
            static_cast<size_t>(ARRAYSIZE(pShm->Apps)),
            sizeof(SMARTMAX_APP_ENTRY));

        int nFound = 0;
        for (DWORD i = 0; i < ARRAYSIZE(pShm->Apps); ++i)
        {
            const SMARTMAX_APP_ENTRY& entry = pShm->Apps[i];
            if (entry.dwProcessId == 0)
                continue;

            wprintf(L"  [%3lu]  +0x%04zX  PID=%-7lu  %.*s\n",
                i,
                FIELD_OFFSET(SMARTMAX_SHARED, Apps) + i * sizeof(SMARTMAX_APP_ENTRY),
                entry.dwProcessId,
                static_cast<int>(ARRAYSIZE(entry.szProcessName)),
                entry.szProcessName);
            ++nFound;
        }
        wprintf(L"  [+] %d active entries.\n\n", nFound);
    }

    // Display table
    {
        wprintf(L"=== Display table  SHM+0x%04zX  (%zu entries x 0x%03zX bytes) ===\n\n",
            static_cast<size_t>(FIELD_OFFSET(SMARTMAX_SHARED, Displays)),
            static_cast<size_t>(ARRAYSIZE(pShm->Displays)),
            sizeof(SMARTMAX_DISPLAY_ENTRY));

        for (DWORD i = 0; i < ARRAYSIZE(pShm->Displays); ++i)
        {
            const SMARTMAX_DISPLAY_ENTRY& d = pShm->Displays[i];

            if (d.Flags == 0 && d.szDeviceName[0] == L'\0')
                continue;

            size_t stEntryOff = FIELD_OFFSET(SMARTMAX_SHARED, Displays)
                + i * sizeof(SMARTMAX_DISPLAY_ENTRY);

            wprintf(L"[Display slot %lu]  SHM+0x%04zX\n", i, stEntryOff);

            wprintf(L"  Flags             0x%08lX  (cells=%lu  nRows=%lu  nCols=%lu)\n",
                d.Flags, d.Flags, d.nRows, d.nCols);

            wprintf(L"  szDeviceName      %.*s\n",
                static_cast<int>(ARRAYSIZE(d.szDeviceName)), d.szDeviceName);

            wprintf(L"  rcDesktop         {%ld, %ld, %ld, %ld}  (%ld x %ld)%s\n",
                d.rcDesktop.left, d.rcDesktop.top,
                d.rcDesktop.right, d.rcDesktop.bottom,
                d.rcDesktop.right - d.rcDesktop.left,
                d.rcDesktop.bottom - d.rcDesktop.top,
                (d.rcDesktop.right == d.rcDesktop.left || d.rcDesktop.bottom == d.rcDesktop.top)
                ? L"  ** ZERO DIMENSION -- VULN-001 **" : L"");

            DWORD dwCells = d.Flags;
            if (dwCells > ARRAYSIZE(d.cells))
                dwCells = ARRAYSIZE(d.cells);

            wprintf(L"  Grid cells        (%lu valid of max %zu):\n",
                dwCells, ARRAYSIZE(d.cells));
            for (DWORD j = 0; j < dwCells; ++j)
            {
                wprintf(L"    cells[%lu]  {%ld, %ld, %ld, %ld}  (%ld x %ld)\n",
                    j,
                    d.cells[j].left, d.cells[j].top,
                    d.cells[j].right, d.cells[j].bottom,
                    d.cells[j].right - d.cells[j].left,
                    d.cells[j].bottom - d.cells[j].top);
            }

            wprintf(L"  Exclusion zones   (at SHM+0x%04zX, max %zu):\n",
                stEntryOff + FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, exclusionZones),
                ARRAYSIZE(d.exclusionZones));
            {
                int nZones = 0;
                for (DWORD j = 0; j < ARRAYSIZE(d.exclusionZones); ++j)
                {
                    if (IsRectEmpty(&d.exclusionZones[j]))
                        break;
                    wprintf(L"    [%lu]  {%ld, %ld, %ld, %ld}  (%ld x %ld)\n",
                        j,
                        d.exclusionZones[j].left, d.exclusionZones[j].top,
                        d.exclusionZones[j].right, d.exclusionZones[j].bottom,
                        d.exclusionZones[j].right - d.exclusionZones[j].left,
                        d.exclusionZones[j].bottom - d.exclusionZones[j].top);
                    ++nZones;
                }
                if (nZones == 0)
                    wprintf(L"    (none)\n");
            }

            wprintf(L"  MONITORINFO       cbSize=%lu  dwFlags=0x%08lX%s\n",
                d.monInfo.cbSize, d.monInfo.dwFlags,
                (d.monInfo.dwFlags & MONITORINFOF_PRIMARY) ? L"  (PRIMARY)" : L"");
            wprintf(L"    rcMonitor       {%ld, %ld, %ld, %ld}  (%ld x %ld)\n",
                d.monInfo.rcMonitor.left, d.monInfo.rcMonitor.top,
                d.monInfo.rcMonitor.right, d.monInfo.rcMonitor.bottom,
                d.monInfo.rcMonitor.right - d.monInfo.rcMonitor.left,
                d.monInfo.rcMonitor.bottom - d.monInfo.rcMonitor.top);
            wprintf(L"    rcWork          {%ld, %ld, %ld, %ld}  (%ld x %ld)\n",
                d.monInfo.rcWork.left, d.monInfo.rcWork.top,
                d.monInfo.rcWork.right, d.monInfo.rcWork.bottom,
                d.monInfo.rcWork.right - d.monInfo.rcWork.left,
                d.monInfo.rcWork.bottom - d.monInfo.rcWork.top);

            wprintf(L"  rcWorkArea        {%ld, %ld, %ld, %ld}\n",
                d.rcWorkArea.left, d.rcWorkArea.top,
                d.rcWorkArea.right, d.rcWorkArea.bottom);

            DWORD dwPrimarySize = d.dwPackedTaskbarDims & 0x00000FFF;
            DWORD dwHorizSize = (d.dwPackedTaskbarDims >> 12) & 0x00000FFF;
            wprintf(L"  dwPackedTaskbarDims  0x%08lX  (primarySize=%lu  horizSize=%lu)\n",
                d.dwPackedTaskbarDims, dwPrimarySize, dwHorizSize);

            DWORD dwVertSize = d.dwPackedDisplayState & PDS_VERT_SIZE_MASK;
            DWORD dwDispIdx = (d.dwPackedDisplayState & PDS_DISP_IDX_MASK) >> PDS_DISP_IDX_SHIFT;
            DWORD dwEdge = (d.dwPackedDisplayState & PDS_EDGE_MASK) >> PDS_EDGE_SHIFT;
            BOOL  bReady = (d.dwPackedDisplayState & PDS_READY_BIT) != 0;
            static const wchar_t* const k_apwszEdge[] =
            { L"?", L"top", L"left", L"?", L"right", L"?", L"?", L"?", L"bottom" };
            wprintf(L"  dwPackedDisplayState 0x%08lX  (vertSize=%lu  dispIdx=%lu  edge=%s  ready=%s)\n\n",
                d.dwPackedDisplayState, dwVertSize, dwDispIdx,
                (dwEdge < ARRAYSIZE(k_apwszEdge)) ? k_apwszEdge[dwEdge] : L"?",
                bReady ? L"yes" : L"no");
        }
    }
}

static VOID HexDump(
    _In_reads_bytes_(stLen) const BYTE* pData,
    _In_ size_t                         stLen,
    _In_ DWORD                          dwBaseOffset)
{
    if (!pData || !stLen)
        return;

    static constexpr size_t k_stWidth = 16;

    for (size_t stRow = 0; stRow < stLen; stRow += k_stWidth)
    {
        wprintf(L"%08lX  ", dwBaseOffset + static_cast<DWORD>(stRow));

        for (size_t i = 0; i < k_stWidth; ++i)
        {
            if (stRow + i < stLen)
                wprintf(L"%02X ", pData[stRow + i]);
            else
                wprintf(L"   ");
            if (i == 7)
                wprintf(L" ");
        }

        wprintf(L" |");
        for (size_t i = 0; i < k_stWidth && stRow + i < stLen; ++i)
        {
            BYTE by = pData[stRow + i];
            wprintf(L"%c", (by >= 0x20 && by < 0x7F) ? static_cast<wchar_t>(by) : L'.');
        }
        wprintf(L"|\n");
    }
}