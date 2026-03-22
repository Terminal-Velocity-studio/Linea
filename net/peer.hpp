#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <memory>
#include "core/identity.hpp"

namespace vanguard::net {

struct Message {
    std::string sender_id;
    std::string payload;
};

class Peer {
public:
    // Создать ноду на порту
    static core::Result<std::unique_ptr<Peer>> create(uint16_t port);

    // Отправить сообщение
    core::Result<void> send(const std::string& host, uint16_t port,
                             const std::string& sender_id, const std::string& text);

    // Callback на входящее сообщение
    void on_message(std::function<void(Message)> cb);

    // Запустить
    core::Result<void> start();

    // Остановить
    void stop();

    uint16_t port() const;

    ~Peer();

private:
    Peer() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vanguard::net
