#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <filesystem>
#include "core/identity.hpp"

namespace vanguard::storage {

struct Message {
    std::string text;
    std::string sender_id;
    std::time_t timestamp;
};

class MessageStore {
public:
    explicit MessageStore(const std::filesystem::path& path);

    // Добавить сообщение (только из UI потока)
    void add(const std::string& text, const std::string& sender_id);

    // Получить копию всех сообщений (безопасно)
    std::vector<Message> messages() const;

    // Сохранить на диск
    core::Result<void> save() const;

private:
    std::filesystem::path path_;
    std::vector<Message> messages_;

    core::Result<void> load();
};

} // namespace vanguard::storage
