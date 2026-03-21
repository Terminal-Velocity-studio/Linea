#include "reliable.hpp"
#include <sodium.h>
#include <sstream>
#include <iomanip>

namespace vanguard::transport {

// Генерируем уникальный ID сообщения через случайные байты
static std::string generate_id() {
    unsigned char buf[8];
    randombytes_buf(buf, sizeof(buf));
    std::ostringstream oss;
    for (auto b : buf)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

ReliableTransport::ReliableTransport() {}
ReliableTransport::~ReliableTransport() {}

void ReliableTransport::on_status(StatusCallback cb) {
    status_cb_ = cb;
}

std::string ReliableTransport::send(
    const std::string& host, uint16_t port,
    const std::string& payload,
    std::function<void(const std::string&, uint16_t, const std::string&)> raw_send)
{
    std::lock_guard<std::mutex> lock(mtx_);

    OutgoingMessage msg;
    msg.id = generate_id();
    msg.host = host;
    msg.port = port;
    msg.payload = payload;
    msg.attempts = 1;
    msg.last_attempt = std::chrono::steady_clock::now();
    // Следующий retry через 10 секунд
    msg.next_retry = msg.last_attempt + std::chrono::seconds(10);
    msg.status = DeliveryStatus::Sent;

    // Отправляем с ID чтобы получатель мог послать ACK
    raw_send(host, port, "MSG:" + msg.id + ":" + payload);

    pending_[msg.id] = msg;

    if (status_cb_) status_cb_(msg.id, DeliveryStatus::Sent);
    return msg.id;
}

void ReliableTransport::on_ack(const std::string& msg_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = pending_.find(msg_id);
    if (it == pending_.end()) return;

    it->second.status = DeliveryStatus::Delivered;
    if (status_cb_) status_cb_(msg_id, DeliveryStatus::Delivered);
    pending_.erase(it);
}

void ReliableTransport::tick(
    std::function<void(const std::string&, uint16_t, const std::string&)> raw_send)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto now = std::chrono::steady_clock::now();

    for (auto& [id, msg] : pending_) {
        if (msg.status != DeliveryStatus::Sent) continue;
        if (now < msg.next_retry) continue;

        if (msg.attempts >= msg.max_attempts) {
            // Все попытки исчерпаны - в недоставленные
            msg.status = DeliveryStatus::Failed;
            if (status_cb_) status_cb_(id, DeliveryStatus::Failed);
            continue;
        }

        // Повторная попытка
        msg.attempts++;
        msg.last_attempt = now;
        msg.next_retry = now + std::chrono::seconds(10);
        raw_send(msg.host, msg.port, "MSG:" + msg.id + ":" + msg.payload);
    }
}

std::vector<OutgoingMessage> ReliableTransport::failed_messages() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<OutgoingMessage> result;
    for (const auto& [id, msg] : pending_) {
        if (msg.status == DeliveryStatus::Failed)
            result.push_back(msg);
    }
    return result;
}

void ReliableTransport::retry(
    const std::string& msg_id,
    std::function<void(const std::string&, uint16_t, const std::string&)> raw_send)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = pending_.find(msg_id);
    if (it == pending_.end()) return;

    auto& msg = it->second;
    msg.attempts = 1;
    msg.status = DeliveryStatus::Sent;
    msg.last_attempt = std::chrono::steady_clock::now();
    msg.next_retry = msg.last_attempt + std::chrono::seconds(10);
    raw_send(msg.host, msg.port, "MSG:" + msg.id + ":" + msg.payload);

    if (status_cb_) status_cb_(msg_id, DeliveryStatus::Sent);
}

} // namespace vanguard::transport
