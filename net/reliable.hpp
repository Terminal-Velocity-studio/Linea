#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <map>
#include <mutex>
#include <chrono>
#include <vector>
#include "core/identity.hpp"

namespace vanguard::net {

enum class DeliveryStatus { Sent, Delivered, Failed };

struct OutgoingMsg {
    std::string id;
    std::string host;
    uint16_t port;
    std::string payload;
    int attempts = 0;
    int max_attempts = 3;
    std::chrono::steady_clock::time_point next_retry;
    DeliveryStatus status = DeliveryStatus::Sent;
};

class ReliableLayer {
public:
    using SendFn = std::function<void(const std::string&, uint16_t, const std::string&)>;
    using StatusFn = std::function<void(const std::string& id, DeliveryStatus)>;

    explicit ReliableLayer(uint16_t listen_port);

    // Отправить надёжно - возвращает ID сообщения
    std::string send(const std::string& host, uint16_t port,
                     const std::string& payload, SendFn raw_send);

    // Вызвать когда пришёл ACK
    void on_ack(const std::string& msg_id);

    // Вызывать каждый кадр - обрабатывает retry
    void tick(SendFn raw_send);

    // Callback на изменение статуса
    void on_status(StatusFn cb);

private:
    std::mutex mtx_; // Светофор
    std::map<std::string, OutgoingMsg> pending_;
    StatusFn status_cb_;
    uint16_t listen_port_;

    static std::string generate_id();
};

} // namespace vanguard::net
