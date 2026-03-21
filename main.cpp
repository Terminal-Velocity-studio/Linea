#include <windows.h>
#include <thread>
#include "core/identity.hpp"
#include "storage/messages.hpp"
#include "transport/peer.hpp"
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

    // Загрузка истории сообщений
    vanguard::MessageStore store("vanguard_messages.bin");

    // Запуск транспортного слоя на порту 443
    // 443 - HTTPS порт, трафик сложнее заблокировать через DPI
    vanguard::transport::PeerTransport transport(443);

    // Когда приходит сообщение от другой ноды - сохраняем
    transport.on_message([&](vanguard::transport::RawMessage msg) {
        store.add(msg.payload, msg.sender_id);
    });

    // Запускаем транспорт в фоновом потоке
    transport.run();

    // Запуск GUI - передаём transport чтобы UI мог отправлять
    vanguard::ui::run_window(identity, store, transport);

    transport.stop();
    return 0;
}
