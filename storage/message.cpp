#include "message.hpp"
#include <fstream>
#include <mutex>

namespace vanguard::storage {

// Мьютекс - светофор для доступа к messages_
// Красный: кто-то пишет - остальные ждут
// Зелёный: никто не пишет - можно читать/писать
static std::mutex g_mutex;

MessageStore::MessageStore(const std::filesystem::path& path) : path_(path) {
    auto result = load();
    if (!result)
        ; // Файл не существует - начинаем с пустого хранилища
}

void MessageStore::add(const std::string& text, const std::string& sender_id) {
    std::lock_guard<std::mutex> lock(g_mutex); // Красный свет для всех
    Message msg;
    msg.text = text;
    msg.sender_id = sender_id;
    msg.timestamp = std::time(nullptr);
    messages_.push_back(msg);
} // lock_guard уничтожается - Зелёный свет

std::vector<Message> MessageStore::messages() const {
    std::lock_guard<std::mutex> lock(g_mutex); // Красный свет
    return messages_; // Возвращаем КОПИЮ - безопасно
} // Зелёный свет

core::Result<void> MessageStore::save() const {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ofstream f(path_, std::ios::binary);
    if (!f) return std::unexpected("Cannot save messages to: " + path_.string());

    size_t count = messages_.size();
    f.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& msg : messages_) {
        f.write(reinterpret_cast<const char*>(&msg.timestamp), sizeof(msg.timestamp));

        size_t len = msg.sender_id.size();
        f.write(reinterpret_cast<const char*>(&len), sizeof(len));
        f.write(msg.sender_id.data(), len);

        len = msg.text.size();
        f.write(reinterpret_cast<const char*>(&len), sizeof(len));
        f.write(msg.text.data(), len);
    }
    return {};
}

core::Result<void> MessageStore::load() {
    std::ifstream f(path_, std::ios::binary);
    if (!f) return std::unexpected("Cannot load messages from: " + path_.string());

    size_t count = 0;
    f.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!f) return std::unexpected("Corrupt message file");

    messages_.clear();
    for (size_t i = 0; i < count; i++) {
        Message msg;
        f.read(reinterpret_cast<char*>(&msg.timestamp), sizeof(msg.timestamp));

        size_t len = 0;
        f.read(reinterpret_cast<char*>(&len), sizeof(len));
        msg.sender_id.resize(len);
        f.read(msg.sender_id.data(), len);

        f.read(reinterpret_cast<char*>(&len), sizeof(len));
        msg.text.resize(len);
        f.read(msg.text.data(), len);

        if (!f) return std::unexpected("Corrupt message at index " + std::to_string(i));
        messages_.push_back(msg);
    }
    return {};
}

} // namespace vanguard::storage
