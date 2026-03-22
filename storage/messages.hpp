#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <filesystem>

namespace vanguard {

// Одно сообщение в чате
struct Message {
    std::string text;       // Текст сообщения
    std::time_t timestamp;  // Время отправки
    std::string sender_id;  // ID отправителя (наш ID = сообщение себе)
};

// Хранилище сообщений (пока локально, потом добавим сеть)
class MessageStore {
public:
    explicit MessageStore(const std::filesystem::path& path);

    // Добавить сообщение
    void add(const std::string& text, const std::string& sender_id);

    // В MessageStore добавим:
    std::vector<Message> swap_buffer(); // забирает новые сообщения атомарно

    // Получить все сообщения
    std::vector<Message> messages() const;

    // Сохранить на диск
    void save() const;

    // Загрузить с диска
    void load();

private:
    std::filesystem::path path_;
    std::vector<Message> messages_;
};

} // namespace vanguard
