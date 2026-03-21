#pragma once
#include <string>
#include <functional>
#include <cstdint>

namespace vanguard::transport {

// Адрес ноды в сети
struct PeerAddress {
    std::string host; // IP или hostname
    uint16_t port;    // Порт
};

// Входящее сообщение от другой ноды
struct RawMessage {
    std::string sender_id;  // ID отправителя
    std::string payload;    // Данные
};

// Транспортный слой - отправляет и принимает сообщения
class PeerTransport {
public:
    // Запустить на указанном порту
    explicit PeerTransport(uint16_t port);
    ~PeerTransport();

    // Отправить сообщение другой ноде
    void send(const PeerAddress& to, const RawMessage& msg);

    // Callback когда пришло сообщение
    void on_message(std::function<void(RawMessage)> callback);

    // Запустить приём (блокирующий)
    void run();

    // Остановить
    void stop();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace vanguard::transport
