#include <iostream>
#include <string>
#include <windows.h>
#include "core/identity.hpp"
#include "storage/messages.hpp"

// Форматируем время из timestamp в читаемый вид
std::string format_time(std::time_t t) {
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    return buf;
}

int main() {
    // Включаем UTF-8 в консоли Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // --- Загрузка или создание Identity ---
    const std::filesystem::path identity_path = "vanguard_identity.bin";
    vanguard::Identity identity;

    if (vanguard::Identity::exists(identity_path)) {
        identity = vanguard::Identity::load(identity_path);
    } else {
        identity = vanguard::Identity::generate();
        identity.save(identity_path);
        std::cout << "New identity created!" << std::endl;
    }

    // --- Загрузка истории сообщений ---
    // Пока это "избранное" - чат с собой (Тайлер Дёрден режим)
    // Потом здесь будет выбор собеседника по ID
    vanguard::MessageStore store("vanguard_messages.bin");

    // --- Приветствие ---
    std::cout << "=== Vanguard ===" << std::endl;
    std::cout << "Your ID: " << identity.id().substr(0, 16) << "..." << std::endl;
    std::cout << "--- Saved messages ---" << std::endl;

    // Показываем историю
    for (const auto& msg : store.messages()) {
        std::cout << "[" << format_time(msg.timestamp) << "] " << msg.text << std::endl;
    }

    std::cout << "--- Type message, /quit to exit ---" << std::endl;

    // --- Основной цикл чата ---
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input == "/quit") break;
        if (input.empty()) continue;

        // Сохраняем сообщение (sender_id = наш ID)
        store.add(input, identity.id());

        // Показываем обратно с временем
        std::cout << "[" << format_time(std::time(nullptr)) << "] " << input << std::endl;
    }

    return 0;
}
