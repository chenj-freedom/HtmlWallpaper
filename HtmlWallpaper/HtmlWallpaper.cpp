#include <windows.h>
#include <wrl.h>
#include <string>
#include <WebView2.h>

using namespace Microsoft::WRL;

// --------------------------------------------------
// Globals
// --------------------------------------------------

HINSTANCE g_hInst = nullptr;
HWND g_hWnd = nullptr;

ComPtr<ICoreWebView2Controller> g_webviewController;
ComPtr<ICoreWebView2> g_webview;

#define ID_TIMER_ESC 1

// --------------------------------------------------
// Utils
// --------------------------------------------------

std::wstring GetHtmlUri()
{
#ifdef _DEBUG
    // Debug: fixed development path
    return L"file:///C:/workspaces/my_work/HtmlWallpaper/index.html";
#else
    // Release: exe directory /index.html
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));

    std::wstring htmlPath = exeDir + L"\\index.html";

    for (auto& ch : htmlPath)
    {
        if (ch == L'\\')
            ch = L'/';
    }

    return L"file:///" + htmlPath;
#endif
}

// --------------------------------------------------
// Fullscreen Helper
// --------------------------------------------------

void SetFullscreenTopmost(HWND hWnd)
{
    // Get full screen size (primary monitor)
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    // Set window to topmost and fullscreen
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, screenW, screenH,
        SWP_SHOWWINDOW);
}

// --------------------------------------------------
// WebView2
// --------------------------------------------------

void InitWebView()
{
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        nullptr,
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                env->CreateCoreWebView2Controller(
                    g_hWnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (controller)
                            {
                                g_webviewController = controller;
                                controller->get_CoreWebView2(&g_webview);

                                RECT bounds;
                                GetClientRect(g_hWnd, &bounds);
                                controller->put_Bounds(bounds);

                                std::wstring uri = GetHtmlUri();
                                g_webview->Navigate(uri.c_str());
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

// --------------------------------------------------
// Window Proc
// --------------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        // Start timer to check ESC key
        SetTimer(hWnd, ID_TIMER_ESC, 100, nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == ID_TIMER_ESC)
        {
            // Check ESC key state (works even when WebView2 has focus)
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                DestroyWindow(hWnd);
            }
        }
        return 0;

    case WM_SIZE:
        if (g_webviewController)
        {
            RECT bounds;
            GetClientRect(hWnd, &bounds);
            g_webviewController->put_Bounds(bounds);
        }
        return 0;

    case WM_ACTIVATE:
        // When activated or deactivated, ensure we stay topmost and fullscreen
        SetFullscreenTopmost(hWnd);
        return 0;

    case WM_DISPLAYCHANGE:
        // Screen resolution changed
        SetFullscreenTopmost(hWnd);
        return 0;

    case WM_KEYDOWN:
        // Safety exit: ESC
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER_ESC);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// --------------------------------------------------
// WinMain
// --------------------------------------------------

int APIENTRY wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPWSTR,
    int)
{
    g_hInst = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"HtmlWallpaperWindow";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Create fullscreen borderless topmost window
    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST,          // Always on top
        wc.lpszClassName,
        L"",
        WS_POPUP,               // No border, no title bar
        0, 0,
        screenW, screenH,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    // Ensure topmost and fullscreen
    SetFullscreenTopmost(g_hWnd);

    InitWebView();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
