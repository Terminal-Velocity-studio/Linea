#include "window.hpp"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace vanguard::ui {

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRTV = nullptr;
static HWND                     g_hwnd = nullptr;

static void CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRTV);
    back->Release();
}

static void CleanupRenderTarget() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
}

static bool InitD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &fl, &g_pd3dContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice && wp != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void run_window(vanguard::Identity& identity, vanguard::MessageStore& store) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0,
        GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"Vanguard", nullptr };
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowW(L"Vanguard", L"Vanguard",
        WS_OVERLAPPEDWINDOW, 100, 100, 900, 600,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!InitD3D(g_hwnd)) return;

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Тёмная тема
    ImGui::StyleColorsDark();

    // Загружаем NotoSans с кириллицей и латиницей
    static const ImWchar noto_ranges[] = {
        0x0020, 0x00FF, // Latin
        0x0400, 0x04FF, // Cyrillic
        0,
    };
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    io.Fonts->AddFontFromFileTTF(
        "C:/Users/baisa/Vanguard/assets/fonts/NotoSans-Regular.ttf", 16.0f, &font_cfg, noto_ranges);;

    // Добавляем NotoEmoji поверх (merge mode)
    static const ImWchar emoji_ranges[] = {
        0x2300, 0x27BF, // Misc symbols
        0x1F300, 0x1F9FF, // Emoji
        0,
    };
    ImFontConfig emoji_cfg;
    emoji_cfg.MergeMode = true; // Мержим с предыдущим шрифтом
    emoji_cfg.OversampleH = 1;
    emoji_cfg.OversampleV = 1;
    io.Fonts->AddFontFromFileTTF(
        "C:/Users/baisa/Vanguard/assets/fonts/NotoEmoji-Regular.ttf", 16.0f, &emoji_cfg, emoji_ranges);


    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    char input_buf[512] = {};
    bool scroll_to_bottom = true;

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove);

        // Заголовок
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Vanguard");
        ImGui::SameLine();
        ImGui::TextDisabled("| Saved | ID: %s...", identity.id().substr(0, 16).c_str());
        ImGui::Separator();

        // Область сообщений
        float input_height = 45.0f;
        ImGui::BeginChild("##messages", {0, -input_height}, false);

        for (const auto& m : store.messages()) {
            char time_buf[32];
            std::strftime(time_buf, sizeof(time_buf), "%H:%M",
                std::localtime(&m.timestamp));
            ImGui::TextDisabled("[%s]", time_buf);
            ImGui::SameLine();
            ImGui::TextWrapped("%s", m.text.c_str());
        }

        if (scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            scroll_to_bottom = false;
        }

        ImGui::EndChild();
        ImGui::Separator();

        // Поле ввода
        ImGui::SetNextItemWidth(-70);
        bool enter = ImGui::InputText("##input", input_buf, sizeof(input_buf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();

        if ((ImGui::Button("Send", {60, 0}) || enter) && input_buf[0]) {
            store.add(std::string(input_buf), identity.id());
            input_buf[0] = '\0';
            scroll_to_bottom = true;
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::End();

        ImVec4 clear = {0.08f, 0.08f, 0.08f, 1.0f};
        g_pd3dContext->ClearRenderTargetView(g_mainRTV, (float*)&clear);
        g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupRenderTarget();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pd3dContext) g_pd3dContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    DestroyWindow(g_hwnd);
    UnregisterClassW(L"Vanguard", wc.hInstance);
}

} // namespace vanguard::ui
