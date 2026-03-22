#pragma once
#include <string>
#include <functional>
#include <cstdint>

namespace vanguard::transport {

struct PeerAddress {
    std::string host;
    uint16_t port;
};

struct RawMessage {
    std::string sender_id;
    std::string payload;
};

class PeerTransport {
public:
    explicit PeerTransport(uint16_t port);
    ~PeerTransport();

    void send(const PeerAddress& to, const RawMessage& msg);
    void on_message(std::function<void(RawMessage)> callback);
    void raw_send(const std::string& host, uint16_t port, const std::string& data);
    void run();
    void stop();

private:
    struct Impl;
    Impl* impl_;
};

} // namespace vanguard::transport
