#include "window.hpp"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <algorithm>

static void ui_log(const std::string& msg) {
    std::ofstream f("vanguard_log.txt", std::ios::app);
    f << "[UI] " << msg << "\n";
}

namespace vanguard::ui {

static std::string format_time(std::time_t t) {
    char buf[32];
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    std::strftime(buf, sizeof(buf), "%H:%M", &tm_info);
    return buf;
}

static std::string find_font(const std::string& name) {
    std::filesystem::path font = std::filesystem::current_path() / "assets" / "fonts" / name;
    if (std::filesystem::exists(font)) return font.string();
    return "";
}

static void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding  = 8.0f;
    s.FrameRounding   = 6.0f;
    s.WindowBorderSize = 0.0f;
    s.FramePadding    = {10, 6};
    s.ItemSpacing     = {8, 6};
    s.ScrollbarSize   = 8.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]      = {0.09f, 0.10f, 0.12f, 1.0f};
    c[ImGuiCol_ChildBg]       = {0.07f, 0.08f, 0.10f, 1.0f};
    c[ImGuiCol_Header]        = {0.17f, 0.35f, 0.55f, 0.8f};
    c[ImGuiCol_HeaderHovered] = {0.20f, 0.40f, 0.65f, 1.0f};
    c[ImGuiCol_Button]        = {0.17f, 0.35f, 0.55f, 1.0f};
    c[ImGuiCol_ButtonHovered] = {0.20f, 0.45f, 0.70f, 1.0f};
    c[ImGuiCol_FrameBg]       = {0.13f, 0.14f, 0.17f, 1.0f};
    c[ImGuiCol_Separator]     = {0.20f, 0.22f, 0.27f, 1.0f};
    c[ImGuiCol_Text]          = {0.90f, 0.92f, 0.95f, 1.0f};
    c[ImGuiCol_TextDisabled]  = {0.45f, 0.48f, 0.55f, 1.0f};
}

void run_window(vanguard::Identity& identity, vanguard::MessageStore& store,
                vanguard::transport::PeerTransport& transport, IncomingQueue& queue) {

    if (!SDL_Init(SDL_INIT_VIDEO)) return;

    SDL_Window* window = SDL_CreateWindow("Vanguard", 960, 640,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) { SDL_Quit(); return; }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return; }

    ui_log("SDL3 initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    apply_style();

    static const ImWchar noto_ranges[] = { 0x0020, 0x00FF, 0x0400, 0x04FF, 0 };
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    std::string noto_path = find_font("NotoSans-Regular.ttf");
    if (!noto_path.empty()) {
        io.Fonts->AddFontFromFileTTF(noto_path.c_str(), 15.0f, &font_cfg, noto_ranges);
        ui_log("NotoSans loaded");
    }

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    ui_log("ImGui SDL3 initialized");

    struct Peer { std::string host; uint16_t port; };
    std::vector<Peer> peers;
    char peer_input[128] = {};
    char input_buf[512] = {};
    int selected_peer = -1;
    bool running = true;
    std::vector<vanguard::Message> local_msgs;
    try { local_msgs = store.messages(); } catch (...) { ui_log("msgs copy failed"); }
    bool msgs_dirty = false;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
        }

        // Сцепление - забираем новые сообщения ДО кадра
        {
            vanguard::transport::RawMessage incoming;
            while (queue.pop(incoming)) {
                store.add(incoming.payload, incoming.sender_id);
                msgs_dirty = true;
                ui_log("Received: " + incoming.payload);
            }
            if (msgs_dirty) {
                std::vector<vanguard::Message> local_msgs;
                try { local_msgs = store.messages(); } catch (...) { ui_log("msgs copy failed"); }
                msgs_dirty = false;
            }
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        float sw = 220.0f;

        // Левая панель
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({sw, (float)h});
        ImGui::Begin("##sidebar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        ImGui::Spacing();
        ImGui::TextColored({0.40f, 0.75f, 1.0f, 1.0f}, "  Vanguard");
        ImGui::TextDisabled("  %.8s...", identity.id().c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Selectable("  Saved Messages", selected_peer == -1, 0, {0, 28}))
            selected_peer = -1;

        ImGui::Spacing();
        ImGui::TextDisabled("  Peers");
        ImGui::Spacing();

        for (int i = 0; i < (int)peers.size(); i++) {
            std::string label = "  " + peers[i].host + ":" + std::to_string(peers[i].port);
            if (ImGui::Selectable(label.c_str(), selected_peer == i, 0, {0, 28})) {
                selected_peer = i;
                std::vector<vanguard::Message> local_msgs;
                try { local_msgs = store.messages(); } catch (...) { ui_log("msgs copy failed"); }
            }
        }

        // Поле добавления пира внизу
        ImGui::SetCursorPosY((float)h - 48);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetNextItemWidth(sw - 48);
        ImGui::InputText("##pi", peer_input, sizeof(peer_input));
        ImGui::SameLine();
        if (ImGui::Button("+", {28, 0}) && peer_input[0]) {
            std::string s(peer_input);
            auto sep = s.find(':');
            if (sep != std::string::npos && sep > 0) {
                try { peers.push_back({s.substr(0, sep), (uint16_t)std::stoi(s.substr(sep+1))}); }
                catch (...) {}
                peer_input[0] = '\0';
            }
        }
        ImGui::End();

        // Правая панель - чат
        ImGui::SetNextWindowPos({sw, 0});
        ImGui::SetNextWindowSize({(float)w - sw, (float)h});
        ImGui::Begin("##chat", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Spacing();
        if (selected_peer == -1)
            ImGui::TextColored({0.40f, 0.75f, 1.0f, 1.0f}, "Saved Messages");
        else
            ImGui::TextColored({0.40f, 0.75f, 1.0f, 1.0f}, "%s:%d",
                peers[selected_peer].host.c_str(), peers[selected_peer].port);
        ImGui::Spacing();
        ImGui::Separator();

        // Область сообщений - простая без BeginChild пузырей
        ImGui::BeginChild("##msgs", {0, -52.0f}, false);

        for (const auto& m : local_msgs) {
            if (m.text.empty()) continue;
            bool is_mine = (m.sender_id == identity.id());
            std::string t = format_time(m.timestamp);

            if (is_mine) {
                ImGui::TextDisabled("%s", t.c_str());
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.6f, 0.85f, 1.0f, 1.0f});
                ImGui::TextUnformatted(m.text.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("%s", t.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(m.text.c_str());
            }
        }

        // Автоскролл вниз
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::Separator();

        // Поле ввода
        ImGui::Spacing();
        ImGui::SetNextItemWidth((float)w - sw - 88);
        bool enter = ImGui::InputText("##input", input_buf, sizeof(input_buf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();

        if ((ImGui::Button("Send", {70, 0}) || enter) && input_buf[0]) {
            std::string text(input_buf);
            try {
                if (selected_peer == -1) {
                    store.add(text, identity.id());
                } else {
                    transport.send({peers[selected_peer].host, peers[selected_peer].port},
                                   {identity.id(), text});
                    store.add(text, identity.id());
                }
                std::vector<vanguard::Message> local_msgs;
                try { local_msgs = store.messages(); } catch (...) { ui_log("msgs copy failed"); }
                ui_log("Sent: " + text);
            } catch (...) {
                ui_log("Send failed");
            }
            input_buf[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::End();

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    store.save();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

} // namespace vanguard::ui
