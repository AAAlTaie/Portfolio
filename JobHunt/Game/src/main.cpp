#include <Windows.h>
#include <windowsx.h>
#include <string>
#include <chrono>
#include <thread>
#include "GraphicsEngine.h"
#include "Renderer.h"

using namespace GraphicsEngine;

static Renderer* gRenderer = nullptr;
static uint32_t gClientWidth = 1280, gClientHeight = 720;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_SIZE: gClientWidth = LOWORD(lParam); gClientHeight = HIWORD(lParam); if (gRenderer) gRenderer->Resize(gClientWidth, gClientHeight); return 0;
    case WM_KEYDOWN: if (gRenderer) gRenderer->OnKeyDown(wParam); if (wParam == VK_ESCAPE) DestroyWindow(hWnd); return 0;
    case WM_KEYUP:   if (gRenderer) gRenderer->OnKeyUp(wParam); return 0;
    case WM_MOUSEMOVE: { bool lmb = (wParam & MK_LBUTTON) != 0; bool rmb = (wParam & MK_RBUTTON) != 0; int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam); if (gRenderer) gRenderer->OnMouseMove(x, y, lmb, rmb); return 0; }
    case WM_MOUSEWHEEL: if (gRenderer) gRenderer->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam)); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"Dx12SolV5"; RegisterClassExW(&wc);
    RECT rc{ 0,0,(LONG)gClientWidth,(LONG)gClientHeight }; AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Dx12 + Engine Prototype (V5 - Optimized)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);
    gRenderer = CreateRenderer();
    if (!gRenderer->Initialize(hwnd, gClientWidth, gClientHeight)) {
        DestroyRenderer(gRenderer);
        MessageBoxW(nullptr, L"Renderer init failed", L"Error", MB_ICONERROR);
        return -1;
    }

    auto prevUpdate = std::chrono::high_resolution_clock::now();
    MSG msg{};
    bool running = true;

    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!running) break;

        auto nowUpdate = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> dtUpdate = nowUpdate - prevUpdate;
        prevUpdate = nowUpdate;

        gRenderer->Update((float)dtUpdate.count());
        gRenderer->Render();

        std::this_thread::yield();
    }

    gRenderer->Shutdown();
    DestroyRenderer(gRenderer);
    gRenderer = nullptr;
    return 0;
}