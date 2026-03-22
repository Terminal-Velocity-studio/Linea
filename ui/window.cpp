#include "window.hpp"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <ctime>
#include <filesystem>

namespace vanguard::ui {

static std::string format_time(std::time_t t) {
    char buf[16];
    struct tm tm_info;
    localtime_s(&tm_info, &t);
    std::strftime(buf, sizeof(buf), "%H:%M", &tm_info);
    return buf;
}

static std::string find_font(const std::string& name) {
    namespace fs = std::filesystem;
    fs::path p = fs::current_path() / "assets" / "fonts" / name;
    return fs::exists(p) ? p.string() : "";
}

static void setup_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding   = 6.0f;
    s.FrameRounding    = 5.0f;
    s.WindowBorderSize = 0.0f;
    s.FramePadding     = {8, 5};
    s.ItemSpacing      = {8, 5};

    auto& c = s.Colors;
    c[ImGuiCol_WindowBg]      = {0.10f, 0.11f, 0.13f, 1.0f};
    c[ImGuiCol_ChildBg]       = {0.08f, 0.09f, 0.11f, 1.0f};
    c[ImGuiCol_FrameBg]       = {0.14f, 0.15f, 0.18f, 1.0f};
    c[ImGuiCol_FrameBgHovered]= {0.18f, 0.19f, 0.23f, 1.0f};
    c[ImGuiCol_Button]        = {0.18f, 0.36f, 0.56f, 1.0f};
    c[ImGuiCol_ButtonHovered] = {0.22f, 0.44f, 0.68f, 1.0f};
    c[ImGuiCol_Header]        = {0.18f, 0.36f, 0.56f, 0.7f};
    c[ImGuiCol_HeaderHovered] = {0.22f, 0.44f, 0.68f, 1.0f};
    c[ImGuiCol_Separator]     = {0.22f, 0.24f, 0.28f, 1.0f};
    c[ImGuiCol_Text]          = {0.88f, 0.90f, 0.94f, 1.0f};
    c[ImGuiCol_TextDisabled]  = {0.44f, 0.47f, 0.54f, 1.0f};
    c[ImGuiCol_ScrollbarBg]   = {0.08f, 0.09f, 0.11f, 1.0f};
    c[ImGuiCol_ScrollbarGrab] = {0.24f, 0.26f, 0.31f, 1.0f};
}

void run(core::Identity& identity, storage::MessageStore& store,
         net::Peer& peer, IncomingQueue& queue) {

    if (!SDL_Init(SDL_INIT_VIDEO)) return;

    SDL_Window* win = SDL_CreateWindow("Vanguard",
        960, 640, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) { SDL_Quit(); return; }

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) { SDL_DestroyWindow(win); SDL_Quit(); return; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    setup_style();

    // Загружаем шрифт
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0400, 0x04FF, 0};
    ImFontConfig fcfg;
    fcfg.OversampleH = 2;
    fcfg.OversampleV = 2;
    auto noto = find_font("NotoSans-Regular.ttf");
    if (!noto.empty())
        io.Fonts->AddFontFromFileTTF(noto.c_str(), 15.0f, &fcfg, ranges);

    ImGui_ImplSDL3_InitForSDLRenderer(win, ren);
    ImGui_ImplSDLRenderer3_Init(ren);

    struct PeerEntry { std::string host; uint16_t port; };
    std::vector<PeerEntry> peers;
    char peer_buf[128] = {};
    char input_buf[512] = {};
    int selected = -1; // -1 = Saved Messages
    bool running = true;

    // Локальная копия сообщений — обновляется безопасно
    std::vector<storage::Message> msgs = store.messages();

    while (running) {
        // Обработка событий
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
        }

        // Светофор — забираем новые сообщения ДО кадра
        {
            net::Message inc;
            bool got_new = false;
            while (queue.pop(inc)) {
                store.add(inc.payload, inc.sender_id);
                got_new = true;
            }
            if (got_new)
                auto new_msgs = store.messages();
                msgs = store.messages(); // Обновляем копию
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        const float SW = 230.0f; // ширина сайдбара

        // ── Левая панель ──────────────────────────────────
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({SW, (float)h});
        ImGui::Begin("##side", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        ImGui::Spacing();
        ImGui::TextColored({0.35f, 0.72f, 1.0f, 1.0f}, " Vanguard");
        ImGui::TextDisabled(" %.8s...", identity.id().c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Saved Messages
        if (ImGui::Selectable(" Saved Messages", selected == -1, 0, {0, 26}))
            selected = -1;

        ImGui::Spacing();
        ImGui::TextDisabled(" Peers");
        ImGui::Spacing();

        for (int i = 0; i < (int)peers.size(); i++) {
            std::string lbl = " " + peers[i].host + ":" + std::to_string(peers[i].port);
            if (ImGui::Selectable(lbl.c_str(), selected == i, 0, {0, 26})) {
                selected = i;
                msgs = store.messages();
            }
        }

        // Поле добавления пира — прибито к низу
        ImGui::SetCursorPosY((float)h - 46);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetNextItemWidth(SW - 46);
        ImGui::InputText("##pb", peer_buf, sizeof(peer_buf));
        ImGui::SameLine();
        if (ImGui::Button("+", {26, 0}) && peer_buf[0]) {
            std::string s(peer_buf);
            auto colon = s.find(':');
            if (colon != std::string::npos && colon > 0) {
                try {
                    peers.push_back({s.substr(0, colon),
                                     (uint16_t)std::stoi(s.substr(colon + 1))});
                } catch (...) {}
                peer_buf[0] = '\0';
            }
        }
        ImGui::End();

        // ── Правая панель (чат) ───────────────────────────
        ImGui::SetNextWindowPos({SW, 0});
        ImGui::SetNextWindowSize({(float)w - SW, (float)h});
        ImGui::Begin("##chat", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove);

        // Заголовок
        ImGui::Spacing();
        if (selected == -1)
            ImGui::TextColored({0.35f, 0.72f, 1.0f, 1.0f}, "Saved Messages");
        else
            ImGui::TextColored({0.35f, 0.72f, 1.0f, 1.0f}, "%s:%d",
                peers[selected].host.c_str(), peers[selected].port);
        ImGui::Spacing();
        ImGui::Separator();

        // Список сообщений
        ImGui::BeginChild("##msgs", {0, -50.0f}, false);
        for (const auto& m : msgs) {
            if (m.text.empty()) continue;
            bool mine = (m.sender_id == identity.id());

            // Время
            ImGui::TextDisabled("[%s]", format_time(m.timestamp).c_str());
            ImGui::SameLine();

            // Текст — мои сообщения голубые
            if (mine) ImGui::PushStyleColor(ImGuiCol_Text, {0.55f, 0.83f, 1.0f, 1.0f});
            ImGui::TextUnformatted(m.text.c_str());
            if (mine) ImGui::PopStyleColor();
        }
        // Автоскролл вниз
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 30)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::Separator();

        // Поле ввода
        ImGui::Spacing();
        ImGui::SetNextItemWidth((float)w - SW - 86);
        bool enter = ImGui::InputText("##inp", input_buf, sizeof(input_buf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();

        if ((ImGui::Button("Send", {68, 0}) || enter) && input_buf[0]) {
            std::string text(input_buf);
            if (selected == -1) {
                store.add(text, identity.id());
            } else {
                peer.send(peers[selected].host, peers[selected].port,
                          identity.id(), text);
                store.add(text, identity.id());
            }
            msgs = store.messages();
            input_buf[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::End();

        // Рендер
        ImGui::Render();
        SDL_SetRenderDrawColor(ren, 18, 19, 22, 255);
        SDL_RenderClear(ren);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), ren);
        SDL_RenderPresent(ren);
    }

    store.save();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

} // namespace vanguard::ui
