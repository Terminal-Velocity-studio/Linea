#include "reliable.hpp"
#include <sodium.h>
#include <sstream>
#include <iomanip>

namespace vanguard::net {

std::string ReliableLayer::generate_id() {
    unsigned char buf[8];
    randombytes_buf(buf, sizeof(buf));
    std::ostringstream oss;
    for (auto b : buf)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}

ReliableLayer::ReliableLayer(uint16_t listen_port)
    : listen_port_(listen_port) {}

void ReliableLayer::on_status(StatusFn cb) { status_cb_ = cb; }

std::string ReliableLayer::send(const std::string& host, uint16_t port,
                                 const std::string& payload, SendFn raw_send) {
    std::lock_guard<std::mutex> lock(mtx_);

    OutgoingMsg msg;
    msg.id = generate_id();
    msg.host = host;
    msg.port = port;
    msg.payload = payload;
    msg.attempts = 1;
    msg.next_retry = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    msg.status = DeliveryStatus::Sent;

    // Формат: MSG:id:listen_port:payload
    raw_send(host, port, "MSG:" + msg.id + ":" + std::to_string(listen_port_) + ":" + payload);

    pending_[msg.id] = msg;
    if (status_cb_) status_cb_(msg.id, DeliveryStatus::Sent);
    return msg.id;
}

void ReliableLayer::on_ack(const std::string& msg_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = pending_.find(msg_id);
    if (it == pending_.end()) return;
    it->second.status = DeliveryStatus::Delivered;
    if (status_cb_) status_cb_(msg_id, DeliveryStatus::Delivered);
    pending_.erase(it);
}

void ReliableLayer::tick(SendFn raw_send) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto now = std::chrono::steady_clock::now();

    for (auto& [id, msg] : pending_) {
        if (msg.status != DeliveryStatus::Sent) continue;
        if (now < msg.next_retry) continue;

        if (msg.attempts >= msg.max_attempts) {
            msg.status = DeliveryStatus::Failed;
            if (status_cb_) status_cb_(id, DeliveryStatus::Failed);
            continue;
        }

        msg.attempts++;
        msg.next_retry = now + std::chrono::seconds(10);
        raw_send(msg.host, msg.port,
            "MSG:" + msg.id + ":" + std::to_string(listen_port_) + ":" + msg.payload);
    }
}

} // namespace vanguard::net
