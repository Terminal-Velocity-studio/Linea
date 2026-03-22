#include "peer.hpp"
#include "reliable.hpp"
#include <asio.hpp>
#include <thread>
#include <fstream>

static void log(const std::string& msg) {
    std::ofstream f("vanguard_log.txt", std::ios::app);
    f << msg << "\n";
}

namespace vanguard::transport {

struct PeerTransport::Impl {
    asio::io_context io_ctx;
    asio::ip::udp::socket socket;
    uint16_t port;
    std::function<void(RawMessage)> callback;
    std::thread io_thread;
    std::array<char, 65536> recv_buf;
    asio::ip::udp::endpoint remote_ep;
    ReliableTransport reliable;

    Impl(uint16_t p)
        : socket(io_ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), p))
        , port(p)
        , reliable(p) {}
};

PeerTransport::PeerTransport(uint16_t port) : impl_(new Impl(port)) {
    log("Transport started on port " + std::to_string(port));
}

PeerTransport::~PeerTransport() { stop(); delete impl_; }

void PeerTransport::on_message(std::function<void(RawMessage)> cb) {
    impl_->callback = cb;
}

void PeerTransport::raw_send(const std::string& host, uint16_t port, const std::string& data) {
    try {
        auto ep = asio::ip::udp::endpoint(asio::ip::make_address(host), port);
        impl_->socket.send_to(asio::buffer(data), ep);
    } catch (std::exception& e) {
        log("raw_send error: " + std::string(e.what()));
    }
}

void PeerTransport::send(const PeerAddress& to, const RawMessage& msg) {
    impl_->reliable.send(to.host, to.port, msg.sender_id + "|" + msg.payload,
        [this](const std::string& h, uint16_t p, const std::string& d) {
            raw_send(h, p, d);
        });
}

void PeerTransport::run() {
    std::function<void()> do_recv = [&]() {
        impl_->socket.async_receive_from(
            asio::buffer(impl_->recv_buf), impl_->remote_ep,
            [&, do_recv](std::error_code ec, size_t bytes) {
                if (!ec && bytes > 0) {
                    std::string data(impl_->recv_buf.data(), bytes);

                    if (data.size() > 4 && data.substr(0, 4) == "MSG:") {
                        try {
                            auto id_end = data.find(':', 4);
                            if (id_end == std::string::npos) { do_recv(); return; }
                            std::string msg_id = data.substr(4, id_end - 4);

                            auto port_end = data.find(':', id_end + 1);
                            if (port_end == std::string::npos) { do_recv(); return; }
                            uint16_t sender_port = (uint16_t)std::stoi(
                                data.substr(id_end + 1, port_end - id_end - 1));

                            std::string rest = data.substr(port_end + 1);
                            log("MSG rest: [" + rest.substr(0, 48) + "]");

                            std::string sender_host = impl_->remote_ep.address().to_string();
                            raw_send(sender_host, sender_port, "ACK:" + msg_id);

                            auto sep = rest.find('|');
                            if (sep == std::string::npos) {
                                log("ERROR: no pipe in rest");
                                do_recv(); return;
                            }

                            RawMessage msg;
                            msg.sender_id = rest.substr(0, sep);
                            msg.payload = rest.substr(sep + 1);
                            log("Parsed: sender=" + msg.sender_id.substr(0, 8) + " payload=" + msg.payload);

                            if (impl_->callback) {
                                try {
                                    impl_->callback(msg);
                                    log("Callback OK");
                                } catch (std::exception& e) {
                                    log("Callback error: " + std::string(e.what()));
                                } catch (...) {
                                    log("Callback unknown error");
                                }
                            }
                        } catch (std::exception& e) {
                            log("MSG parse error: " + std::string(e.what()));
                        }
                    } else if (data.size() > 4 && data.substr(0, 4) == "ACK:") {
                        impl_->reliable.on_ack(data.substr(4));
                        log("Got ACK: " + data.substr(4, 16));
                    }
                }
                do_recv();
            }
        );
    };

    do_recv();
    impl_->io_thread = std::thread([&]() {
        try { impl_->io_ctx.run(); }
        catch (std::exception& e) { log("IO error: " + std::string(e.what())); }
    });
}

void PeerTransport::stop() {
    impl_->io_ctx.stop();
    if (impl_->io_thread.joinable()) impl_->io_thread.join();
}

} // namespace vanguard::transport
