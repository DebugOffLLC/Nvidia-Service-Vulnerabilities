#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>

static_assert(sizeof(void*) == 8, "x64 Only");

/*
    SMARTMAX_Shared_Memory Layout (nvsmartmax64.dll)

    Total size: 0x4ED0 bytes
      0x0000-0x0047  SMARTMAX_SHARED header
      0x0048-0x41E7  SMARTMAX_APP_ENTRY[200]   (200 x 0x54 bytes)
      0x41E8-0x41EF  MovingTaskbar / pad
      0x41F0-0x4ECF  SMARTMAX_DISPLAY_ENTRY[4] (4 x 0x338 bytes)

    SMARTMAX_SHARED.Flags (+0x0008) - enum SMARTMAX_FLAGS:
      SMF_ENABLED       = 0x00000001  active/running        (cleared in NvSmartMaxShutdown)
      SMF_HOOKS_ACTIVE  = 0x00000002  hooks installed       (set by SmartMax_HookInstallThread)
      SMF_DISPLAY_READY = 0x00000004  display grid ready    (set by NvSmartMaxSetDeviceGrids)
      SMF_HOOK_GATE     = 0x00000007  gate: all three must be set for hooks to fire
      SMF_PHASE_INIT    = 0x00001000  init complete         (set in NvSmartMaxEntryPoint)
      SMF_PHASE_HOOKS   = 0x00004000  hooks phase active
      SMF_PHASE_READY   = 0x00008000  fully operational

    SMARTMAX_DISPLAY_ENTRY.dwPackedTaskbarDims (+0x2E0):
      bits[11: 0]  primary taskbar dimension (px)
      bits[23:12]  horizontal-edge taskbar width (px)

    SMARTMAX_DISPLAY_ENTRY.dwPackedDisplayState (+0x2E4):
      bits[11: 0]  vertical taskbar size (px)
      bits[15:12]  display slot index
      bits[23:20]  taskbar edge (1=top  2=left  4=right  8=bottom)
      bit     26   result ready
*/

struct SMARTMAX_APP_ENTRY {         // 0x54 bytes
    DWORD  dwProcessId;             // +0x00
    WCHAR  szProcessName[40];       // +0x04
};
static_assert(sizeof(SMARTMAX_APP_ENTRY) == 0x54, "SMARTMAX_APP_ENTRY size");
static_assert(FIELD_OFFSET(SMARTMAX_APP_ENTRY, szProcessName) == 0x004, "SMARTMAX_APP_ENTRY::szProcessName");

struct SMARTMAX_DISPLAY_ENTRY {         // 0x338 bytes
    DWORD       Flags;                  // +0x000  rows*cols after BuildDisplayGrid; snap-mode gate bits
    DWORD       nRows;                  // +0x004  grid row count
    DWORD       nCols;                  // +0x008  grid col count
    RECT        rcDesktop;              // +0x00C  virtual desktop rect
    RECT        cells[32];              // +0x01C  grid cell rects (valid [0..Flags-1]); OOB via attacker Flags
    RECT        exclusionZones[8];      // +0x21C  null-terminated (empty RECT = end)
    MONITORINFO monInfo;                // +0x29C  cbSize(=40) + rcMonitor + rcWork + dwFlags
    BYTE        _unk_2C4[0x004];        // +0x2C4
    RECT        rcWorkArea;             // +0x2C8  SmartMax_SetTaskbarRect result; attacker-writable
    BYTE        _unk_2D8[0x008];        // +0x2D8  zeroed after taskbar update; use unknown
    DWORD       dwPackedTaskbarDims;    // +0x2E0  bits[11:0]=primarySize, bits[23:12]=horizSize
    DWORD       dwPackedDisplayState;   // +0x2E4  bits[11:0]=vertSize, bits[15:12]=displayIdx, bits[23:20]=edge, bit26=ready
    WCHAR       szDeviceName[0x28];     // +0x2E8  e.g. L"\\.\DISPLAY1"
};
static_assert(sizeof(SMARTMAX_DISPLAY_ENTRY) == 0x338, "SMARTMAX_DISPLAY_ENTRY size");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, nRows) == 0x004, "SMARTMAX_DISPLAY_ENTRY::nRows");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, nCols) == 0x008, "SMARTMAX_DISPLAY_ENTRY::nCols");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, rcDesktop) == 0x00C, "SMARTMAX_DISPLAY_ENTRY::rcDesktop");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, cells) == 0x01C, "SMARTMAX_DISPLAY_ENTRY::cells");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, exclusionZones) == 0x21C, "SMARTMAX_DISPLAY_ENTRY::exclusionZones");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, monInfo) == 0x29C, "SMARTMAX_DISPLAY_ENTRY::monInfo");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, rcWorkArea) == 0x2C8, "SMARTMAX_DISPLAY_ENTRY::rcWorkArea");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, dwPackedTaskbarDims) == 0x2E0, "SMARTMAX_DISPLAY_ENTRY::dwPackedTaskbarDims");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, dwPackedDisplayState) == 0x2E4, "SMARTMAX_DISPLAY_ENTRY::dwPackedDisplayState");
static_assert(FIELD_OFFSET(SMARTMAX_DISPLAY_ENTRY, szDeviceName) == 0x2E8, "SMARTMAX_DISPLAY_ENTRY::szDeviceName");

struct SMARTMAX_SHARED {                    // 0x4ED0 bytes
    DWORD                InitMask;          // +0x0000  cached OS version mask
    DWORD                _pad_04;
    DWORD                Flags;             // +0x0008  state bitfield (enum SMARTMAX_FLAGS)
    DWORD                _pad_0C;
    HWND                 hwndTrackedFG;     // +0x0010  WM_CLOSE posted here on shutdown
    HWND                 hwndApp;           // +0x0018  nvSmartMaxApp64 main HWND
    DWORD                SetHookArrivals;   // +0x0020  bumped before SetEvent(SMARTMAX_SetHook_Event)
    DWORD                UnHookArrivals;    // +0x0024
    DWORD                SetHookInFlight;   // +0x0028  in-progress refcount
    DWORD                UnHookInFlight;    // +0x002C
    ULONGLONG            TaggedState1;      // +0x0030  top nibble must == 0x5
    ULONGLONG            TaggedState2;      // +0x0038  init = 7
    DWORD                TaggedState3;      // +0x0040  top nibble must == 0x5
    DWORD                State4;            // +0x0044
    SMARTMAX_APP_ENTRY   Apps[200];         // +0x0048  PID -> short process name table
    DWORD                MovingTaskbar;     // +0x41E8  bit 0 set during Shell_TrayWnd SetWindowPos
    DWORD                _pad_41EC;
    SMARTMAX_DISPLAY_ENTRY Displays[4];    // +0x41F0
};
static_assert(sizeof(SMARTMAX_SHARED) == 0x4ED0, "SMARTMAX_SHARED size");
static_assert(FIELD_OFFSET(SMARTMAX_SHARED, Flags) == 0x0008, "SMARTMAX_SHARED::Flags");
static_assert(FIELD_OFFSET(SMARTMAX_SHARED, hwndTrackedFG) == 0x0010, "SMARTMAX_SHARED::hwndTrackedFG");
static_assert(FIELD_OFFSET(SMARTMAX_SHARED, hwndApp) == 0x0018, "SMARTMAX_SHARED::hwndApp");
static_assert(FIELD_OFFSET(SMARTMAX_SHARED, SetHookArrivals) == 0x0020, "SMARTMAX_SHARED::SetHookArrivals");
static_assert(FIELD_OFFSET(SMARTMAX_SHARED, Apps) == 0x0048, "SMARTMAX_SHARED::Apps");
static_assert(FIELD_OFFSET(SMARTMAX_SHARED, MovingTaskbar) == 0x41E8, "SMARTMAX_SHARED::MovingTaskbar");
static_assert(FIELD_OFFSET(SMARTMAX_SHARED, Displays) == 0x41F0, "SMARTMAX_SHARED::Displays");

// SMARTMAX_SHARED.Flags bit masks.
static constexpr DWORD SMF_ENABLED = 0x00000001;
static constexpr DWORD SMF_HOOKS_ACTIVE = 0x00000002;
static constexpr DWORD SMF_DISPLAY_READY = 0x00000004;
static constexpr DWORD SMF_HOOK_GATE = 0x00000007;
static constexpr DWORD SMF_PHASE_INIT = 0x00001000;
static constexpr DWORD SMF_PHASE_HOOKS = 0x00004000;
static constexpr DWORD SMF_PHASE_READY = 0x00008000;

// SMARTMAX_DISPLAY_ENTRY.dwPackedDisplayState bit helpers (+0x2E4)
static constexpr DWORD PDS_VERT_SIZE_MASK = 0x00000FFF;    // bits[11:0]
static constexpr DWORD PDS_DISP_IDX_SHIFT = 12;
static constexpr DWORD PDS_DISP_IDX_MASK = 0x0000F000;    // bits[15:12]
static constexpr DWORD PDS_EDGE_SHIFT = 20;
static constexpr DWORD PDS_EDGE_MASK = 0x00F00000;    // bits[23:20]
static constexpr DWORD PDS_READY_BIT = 0x04000000;    // bit 26
