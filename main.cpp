#include <windows.h>
#include "core/identity.hpp"
#include "storage/messages.hpp"
#include "ui/window.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Загрузка или создание Identity
    const std::filesystem::path identity_path = "vanguard_identity.bin";
    vanguard::Identity identity;

    if (vanguard::Identity::exists(identity_path)) {
        identity = vanguard::Identity::load(identity_path);
    } else {
        identity = vanguard::Identity::generate();
        identity.save(identity_path);
    }

    // Загрузка истории сообщений (чат с собой - Тайлер Дёрден режим)
    vanguard::MessageStore store("vanguard_messages.bin");

    // Запуск GUI
    vanguard::ui::run_window(identity, store);

    return 0;
}
