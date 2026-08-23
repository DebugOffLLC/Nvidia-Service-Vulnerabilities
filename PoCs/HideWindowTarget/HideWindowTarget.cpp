/*
    Example target for CrashWindowOnMove.
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define TARGET_CLASS L"HideWindowTarget"

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT cr, wr;
        GetClientRect(h, &cr);
        GetWindowRect(h, &wr);
        WCHAR buf[160];
        wsprintfW(buf, L"VULN-011 Target   pos=(%d,%d)   size=%dx%d",
            wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top);
        FillRect(hdc, &cr, (HBRUSH)(COLOR_WINDOW + 1));
        DrawTextW(hdc, buf, -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOVE:
    case WM_SIZE:
        InvalidateRect(h, nullptr, TRUE);
        return DefWindowProcW(h, m, w, l);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = TARGET_CLASS;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, TARGET_CLASS, L"VULN-011 Target Window",
        WS_OVERLAPPEDWINDOW, 100, 100, 480, 200,
        nullptr, nullptr, hI, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}