#include "messages.hpp"
#include <fstream>
#include <stdexcept>

namespace vanguard {

MessageStore::MessageStore(const std::filesystem::path& path) : path_(path) {
    // Если файл существует - загружаем сразу
    if (std::filesystem::exists(path_)) {
        load();
    }
}

// Добавляем сообщение с текущим временем
void MessageStore::add(const std::string& text, const std::string& sender_id) {
    Message msg;
    msg.text = text;
    msg.timestamp = std::time(nullptr);
    msg.sender_id = sender_id;
    messages_.push_back(msg);
    save(); // Сохраняем сразу после каждого сообщения
}

const std::vector<Message>& MessageStore::messages() const {
    return messages_;
}

// Простой бинарный формат: [timestamp][sender_id_len][sender_id][text_len][text]
void MessageStore::save() const {
    std::ofstream f(path_, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot save messages");

    size_t count = messages_.size();
    f.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& msg : messages_) {
        // Время
        f.write(reinterpret_cast<const char*>(&msg.timestamp), sizeof(msg.timestamp));

        // sender_id
        size_t sid_len = msg.sender_id.size();
        f.write(reinterpret_cast<const char*>(&sid_len), sizeof(sid_len));
        f.write(msg.sender_id.data(), sid_len);

        // text
        size_t text_len = msg.text.size();
        f.write(reinterpret_cast<const char*>(&text_len), sizeof(text_len));
        f.write(msg.text.data(), text_len);
    }
}

void MessageStore::load() {
    std::ifstream f(path_, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot load messages");

    size_t count = 0;
    f.read(reinterpret_cast<char*>(&count), sizeof(count));

    messages_.clear();
    for (size_t i = 0; i < count; i++) {
        Message msg;

        f.read(reinterpret_cast<char*>(&msg.timestamp), sizeof(msg.timestamp));

        size_t sid_len = 0;
        f.read(reinterpret_cast<char*>(&sid_len), sizeof(sid_len));
        msg.sender_id.resize(sid_len);
        f.read(msg.sender_id.data(), sid_len);

        size_t text_len = 0;
        f.read(reinterpret_cast<char*>(&text_len), sizeof(text_len));
        msg.text.resize(text_len);
        f.read(msg.text.data(), text_len);

        messages_.push_back(msg);
    }
}

} // namespace vanguard
