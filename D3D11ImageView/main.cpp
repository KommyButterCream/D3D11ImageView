#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h> // for Win32 창 및 메시지 API
#include <cstdio>    // for std::fprintf, stderr

namespace
{
    constexpr wchar_t WindowClassName[] = L"D3D11ImageView";

    void ReportError(const char* operation, DWORD error)
    {
        // 정리 작업으로 오류 코드가 바뀌기 전에 실패 지점에서 GetLastError를 전달한다.
        std::fprintf(stderr, "%s failed (Win32 error %lu).\n",
            operation, static_cast<unsigned long>(error));
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_DESTROY:
            // WM_QUIT은 창에 전달되지 않고 스레드의 메시지 루프를 종료한다.
            PostQuitMessage(0);
            return 0;

        default:
            // WM_CLOSE에 따른 창 닫기를 포함한 기본 처리를 맡긴다.
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }
}

int main()
{
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    if (instance == nullptr)
    {
        ReportError("GetModuleHandleW", GetLastError());
        return 1;
    }

    const HCURSOR cursor = LoadCursorW(nullptr, IDC_ARROW);
    if (cursor == nullptr)
    {
        ReportError("LoadCursorW", GetLastError());
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = cursor;
    // 이 브러시와 공유 커서는 시스템 소유이므로 직접 해제하지 않는다.
    windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    windowClass.lpszClassName = WindowClassName;

    if (RegisterClassExW(&windowClass) == 0)
    {
        ReportError("RegisterClassExW", GetLastError());
        return 1;
    }

    constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    RECT windowRect{ 0, 0, 1280, 720 };
    if (!AdjustWindowRectEx(&windowRect, windowStyle, FALSE, 0))
    {
        ReportError("AdjustWindowRectEx", GetLastError());
        UnregisterClassW(WindowClassName, instance);
        return 1;
    }

    const HWND window = CreateWindowExW(
        0, WindowClassName, L"D3D11ImageView - Stage 0", windowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        nullptr, nullptr, instance, nullptr);

    if (window == nullptr)
    {
        ReportError("CreateWindowExW", GetLastError());
        UnregisterClassW(WindowClassName, instance);
        return 1;
    }

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    int exitCode = 0;
    for (;;)
    {
        // GetMessage는 메시지 수신(양수), WM_QUIT(0), 오류(-1)를 구분해야 한다.
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1)
        {
            ReportError("GetMessageW", GetLastError());
            exitCode = 1;
            break;
        }
        if (result == 0)
        {
            exitCode = static_cast<int>(message.wParam);
            break;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    // 정상 종료 시에는 이미 제거됐지만, 오류 경로에서는 창이 남아 있을 수 있다.
    if (IsWindow(window))
        DestroyWindow(window);

    if (!UnregisterClassW(WindowClassName, instance))
    {
        ReportError("UnregisterClassW", GetLastError());
        exitCode = 1;
    }
    return exitCode;
}
