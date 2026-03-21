#include "peer.hpp"
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

    Impl(uint16_t p)
        : socket(io_ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), p))
        , port(p) {}
};

PeerTransport::PeerTransport(uint16_t port)
    : impl_(new Impl(port)) {
    log("Transport started on port " + std::to_string(port));
}

PeerTransport::~PeerTransport() {
    stop();
    delete impl_;
}

void PeerTransport::on_message(std::function<void(RawMessage)> callback) {
    impl_->callback = callback;
}

void PeerTransport::send(const PeerAddress& to, const RawMessage& msg) {
    try {
        auto ep = asio::ip::udp::endpoint(
            asio::ip::make_address(to.host), to.port);
        std::string packet = msg.sender_id + "|" + msg.payload;
        impl_->socket.send_to(asio::buffer(packet), ep);
        log("Sent to " + to.host + ":" + std::to_string(to.port));
    } catch (std::exception& e) {
        log("Send error: " + std::string(e.what()));
    }
}

void PeerTransport::run() {
    std::function<void()> do_recv = [&]() {
        impl_->socket.async_receive_from(
            asio::buffer(impl_->recv_buf),
            impl_->remote_ep,
            [&, do_recv](std::error_code ec, size_t bytes) {
                if (!ec && bytes > 0) {
                    try {
                        std::string data(impl_->recv_buf.data(), bytes);
                        log("Received: " + data.substr(0, 32));
                        auto sep = data.find('|');
                        if (sep != std::string::npos && impl_->callback) {
                            RawMessage msg;
                            msg.sender_id = data.substr(0, sep);
                            msg.payload = data.substr(sep + 1);
                            impl_->callback(msg);
                            log("Callback called OK");
                        }
                    } catch (std::exception& e) {
                        log("Recv callback error: " + std::string(e.what()));
                    }
                }
                do_recv();
            }
        );
    };

    do_recv();
    impl_->io_thread = std::thread([&]() {
        try {
            impl_->io_ctx.run();
        } catch (std::exception& e) {
            log("IO thread error: " + std::string(e.what()));
        }
    });
}

void PeerTransport::stop() {
    impl_->io_ctx.stop();
    if (impl_->io_thread.joinable())
        impl_->io_thread.join();
}

} // namespace vanguard::transport
