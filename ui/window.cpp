#include "window.hpp"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <vector>

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

void run_window(vanguard::Identity& identity, vanguard::MessageStore& store,
                vanguard::transport::PeerTransport& transport) {
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

    ImGui::StyleColorsDark();

    // Шрифт NotoSans с кириллицей
    static const ImWchar noto_ranges[] = {
        0x0020, 0x00FF, // Latin
        0x0400, 0x04FF, // Cyrillic
        0,
    };
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    io.Fonts->AddFontFromFileTTF(
        "C:/Users/baisa/Vanguard/assets/fonts/NotoSans-Regular.ttf",
        16.0f, &font_cfg, noto_ranges);

    // NotoEmoji поверх
    static const ImWchar emoji_ranges[] = {
        0x2300, 0x27BF,
        0,
    };
    ImFontConfig emoji_cfg;
    emoji_cfg.MergeMode = true;
    emoji_cfg.OversampleH = 1;
    emoji_cfg.OversampleV = 1;
    io.Fonts->AddFontFromFileTTF(
        "C:/Users/baisa/Vanguard/assets/fonts/NotoEmoji-Regular.ttf",
        16.0f, &emoji_cfg, emoji_ranges);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    // Список пиров (IP:port)
    // TODO: потом заменить на DHT автообнаружение
    struct Peer {
        std::string host;
        uint16_t port;
        std::string label; // Отображаемое имя
    };
    std::vector<Peer> peers;
    char peer_input[128] = {}; // Поле ввода нового пира "IP:port"

    char input_buf[512] = {};
    bool scroll_to_bottom = true;

    // Текущий выбранный пир (с кем чатимся)
    // -1 = чат с собой (избранное)
    int selected_peer = -1;

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

        // Левая панель — список пиров
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({220, io.DisplaySize.y});
        ImGui::Begin("##peers", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove);

        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Vanguard");
        ImGui::TextDisabled("ID: %.8s...", identity.id().c_str());
        ImGui::Separator();

        // Избранное (чат с собой)
        bool saved_selected = (selected_peer == -1);
        if (ImGui::Selectable("Saved Messages", saved_selected)) {
            selected_peer = -1;
        }

        ImGui::Separator();
        ImGui::TextDisabled("Peers:");

        // Список пиров
        for (int i = 0; i < (int)peers.size(); i++) {
            std::string label = peers[i].host + ":" + std::to_string(peers[i].port);
            bool is_selected = (selected_peer == i);
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_peer = i;
                scroll_to_bottom = true;
            }
        }

        ImGui::Separator();

        // Добавление нового пира
        ImGui::SetNextItemWidth(140);
        ImGui::InputText("##peer_input", peer_input, sizeof(peer_input));
        ImGui::SameLine();
        if (ImGui::Button("+") && peer_input[0]) {
            // Парсим "IP:port"
            std::string s(peer_input);
            auto sep = s.find(':');
            if (sep != std::string::npos) {
                Peer p;
                p.host = s.substr(0, sep);
                p.port = (uint16_t)std::stoi(s.substr(sep + 1));
                peers.push_back(p);
                peer_input[0] = '\0';
            }
        }

        ImGui::End();

        // Правая панель — чат
        ImGui::SetNextWindowPos({220, 0});
        ImGui::SetNextWindowSize({io.DisplaySize.x - 220, io.DisplaySize.y});
        ImGui::Begin("##chat", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove);

        // Заголовок чата
        if (selected_peer == -1) {
            ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Saved Messages");
        } else {
            ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "%s:%d",
                peers[selected_peer].host.c_str(),
                peers[selected_peer].port);
        }
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
            std::string text(input_buf);

            if (selected_peer == -1) {
                // Сохраняем локально
                store.add(text, identity.id());
            } else {
                // Отправляем через transport к выбранному пиру
                vanguard::transport::PeerAddress addr{
                    peers[selected_peer].host,
                    peers[selected_peer].port
                };
                vanguard::transport::RawMessage raw_msg{
                    identity.id(),
                    text
                };
                transport.send(addr, raw_msg);
                // Показываем у себя тоже
                store.add(text, identity.id());
            }

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
