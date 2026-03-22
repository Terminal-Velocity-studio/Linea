#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <chrono>
#include <map>
#include <mutex>
#include <vector>

namespace vanguard::transport {

enum class DeliveryStatus { Pending, Sent, Delivered, Failed };

struct OutgoingMessage {
    std::string id;
    std::string host;
    uint16_t port;
    std::string payload;
    int attempts = 0;
    int max_attempts = 3;
    std::chrono::steady_clock::time_point last_attempt;
    std::chrono::steady_clock::time_point next_retry;
    DeliveryStatus status = DeliveryStatus::Pending;
};

class ReliableTransport {
public:
    using StatusCallback = std::function<void(const std::string& msg_id, DeliveryStatus)>;

    explicit ReliableTransport(uint16_t listen_port);
    ~ReliableTransport();

    std::string send(const std::string& host, uint16_t port,
                     const std::string& payload,
                     std::function<void(const std::string&, uint16_t, const std::string&)> raw_send);

    void on_ack(const std::string& msg_id);
    void tick(std::function<void(const std::string&, uint16_t, const std::string&)> raw_send);
    void on_status(StatusCallback cb);
    std::vector<OutgoingMessage> failed_messages();
    void retry(const std::string& msg_id,
               std::function<void(const std::string&, uint16_t, const std::string&)> raw_send);

private:
    std::mutex mtx_;
    std::map<std::string, OutgoingMessage> pending_;
    StatusCallback status_cb_;
    uint16_t listen_port_;
};

} // namespace vanguard::transport
