#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <chrono>
#include <map>
#include <mutex>

namespace vanguard::transport {

// Статус доставки сообщения
enum class DeliveryStatus {
    Pending,    // Ожидает отправки
    Sent,       // Отправлено, ждём ACK
    Delivered,  // ACK получен
    Failed      // Все попытки исчерпаны
};

// Исходящее сообщение с retry логикой
struct OutgoingMessage {
    std::string id;           // Уникальный ID сообщения
    std::string host;
    uint16_t port;
    std::string payload;
    int attempts = 0;         // Сколько раз пытались
    int max_attempts = 3;     // Максимум попыток
    std::chrono::steady_clock::time_point last_attempt;
    std::chrono::steady_clock::time_point next_retry;
    DeliveryStatus status = DeliveryStatus::Pending;
};

// Надёжная доставка поверх UDP
class ReliableTransport {
public:
    // Callback когда статус сообщения изменился
    using StatusCallback = std::function<void(const std::string& msg_id, DeliveryStatus)>;

    ReliableTransport();
    ~ReliableTransport();

    // Отправить сообщение надёжно
    std::string send(const std::string& host, uint16_t port,
                     const std::string& payload,
                     std::function<void(const std::string&, uint16_t, const std::string&)> raw_send);

    // Вызывать когда пришёл ACK от получателя
    void on_ack(const std::string& msg_id);

    // Вызывать каждый кадр - обрабатывает retry
    void tick(std::function<void(const std::string&, uint16_t, const std::string&)> raw_send);

    // Callback на изменение статуса
    void on_status(StatusCallback cb);

    // Список недоставленных
    std::vector<OutgoingMessage> failed_messages();

    // Повторить недоставленное вручную
    void retry(const std::string& msg_id,
               std::function<void(const std::string&, uint16_t, const std::string&)> raw_send);

private:
    std::mutex mtx_;
    std::map<std::string, OutgoingMessage> pending_;
    StatusCallback status_cb_;
};

} // namespace vanguard::transport
