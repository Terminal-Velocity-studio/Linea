#include "peer.hpp"
#include "socket.hpp"
#include "reliable.hpp"

namespace vanguard::net {

struct Peer::Impl {
    std::unique_ptr<UdpSocket> socket;
    ReliableLayer reliable;
    std::function<void(Message)> msg_callback;
    uint16_t port;

    Impl(uint16_t p, std::unique_ptr<UdpSocket> s)
        : socket(std::move(s)), reliable(p), port(p) {}
};

core::Result<std::unique_ptr<Peer>> Peer::create(uint16_t port) {
    auto sock_result = UdpSocket::create(port);
    if (!sock_result) return std::unexpected(sock_result.error());

    auto peer = std::unique_ptr<Peer>(new Peer());
    peer->impl_ = std::make_unique<Impl>(port, std::move(*sock_result));
    return peer;
}

core::Result<void> Peer::send(const std::string& host, uint16_t port,
                               const std::string& sender_id, const std::string& text) {
    auto raw_send = [this](const std::string& h, uint16_t p, const std::string& d) {
        impl_->socket->send(h, p, d);
    };

    impl_->reliable.send(host, port, sender_id + "|" + text, raw_send);
    return {};
}

void Peer::on_message(std::function<void(Message)> cb) {
    impl_->msg_callback = cb;

    impl_->socket->on_packet([this](Packet pkt) {
        const auto& data = pkt.data;

        if (data.size() > 4 && data.substr(0, 4) == "MSG:") {
            // Формат: MSG:id:listen_port:sender|payload
            try {
                auto id_end = data.find(':', 4);
                if (id_end == std::string::npos) return;
                std::string msg_id = data.substr(4, id_end - 4);

                auto port_end = data.find(':', id_end + 1);
                if (port_end == std::string::npos) return;
                uint16_t sender_port = (uint16_t)std::stoi(
                    data.substr(id_end + 1, port_end - id_end - 1));

                std::string rest = data.substr(port_end + 1);

                // Отправляем ACK на listen_port отправителя
                impl_->socket->send(pkt.from_host, sender_port, "ACK:" + msg_id);

                // Парсим sender|payload
                auto sep = rest.find('|');
                if (sep == std::string::npos) return;

                Message msg;
                msg.sender_id = rest.substr(0, sep);
                msg.payload = rest.substr(sep + 1);

                if (impl_->msg_callback)
                    impl_->msg_callback(msg);

            } catch (...) {}

        } else if (data.size() > 4 && data.substr(0, 4) == "ACK:") {
            impl_->reliable.on_ack(data.substr(4));
        }
    });
}

core::Result<void> Peer::start() {
    return impl_->socket->start();
}

void Peer::stop() {
    impl_->socket->stop();
}

uint16_t Peer::port() const { return impl_->port; }

Peer::~Peer() { stop(); }

} // namespace vanguard::net
