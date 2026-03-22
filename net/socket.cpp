#include "socket.hpp"
#include <asio.hpp>
#include <thread>
#include <array>
#include <memory>

namespace vanguard::net {

struct UdpSocket::Impl {
    asio::io_context io_ctx;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint remote_ep;
    std::array<char, 65536> recv_buf;
    std::function<void(Packet)> callback;
    std::thread io_thread;
    uint16_t port;

    Impl(uint16_t p)
        : socket(io_ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), p))
        , port(p) {}
};

core::Result<std::unique_ptr<UdpSocket>> UdpSocket::create(uint16_t port) {
    try {
        auto sock = std::unique_ptr<UdpSocket>(new UdpSocket());
        sock->impl_ = std::make_unique<Impl>(port);
        return sock;
    } catch (std::exception& e) {
        return std::unexpected(std::string("Failed to create socket: ") + e.what());
    }
}

core::Result<void> UdpSocket::send(const std::string& host, uint16_t port, const std::string& data) {
    try {
        auto ep = asio::ip::udp::endpoint(asio::ip::make_address(host), port);
        impl_->socket.send_to(asio::buffer(data), ep);
        return {};
    } catch (std::exception& e) {
        return std::unexpected(std::string("Send failed: ") + e.what());
    }
}

void UdpSocket::on_packet(std::function<void(Packet)> cb) {
    impl_->callback = cb;
}

core::Result<void> UdpSocket::start() {
    try {
        // Используем shared_ptr чтобы избежать dangling reference
        auto impl_ptr = impl_.get();

        std::function<void()> do_recv;
        auto do_recv_ptr = std::make_shared<std::function<void()>>();

        *do_recv_ptr = [this, do_recv_ptr]() {
            impl_->socket.async_receive_from(
                asio::buffer(impl_->recv_buf), impl_->remote_ep,
                [this, do_recv_ptr](std::error_code ec, size_t bytes) {
                    if (!ec && bytes > 0 && impl_->callback) {
                        Packet pkt;
                        pkt.data = std::string(impl_->recv_buf.data(), bytes);
                        pkt.from_host = impl_->remote_ep.address().to_string();
                        pkt.from_port = impl_->remote_ep.port();
                        try { impl_->callback(pkt); }
                        catch (...) {}
                    }
                    (*do_recv_ptr)();
                }
            );
        };

        (*do_recv_ptr)();

        impl_->io_thread = std::thread([this]() {
            try { impl_->io_ctx.run(); }
            catch (...) {}
        });

        return {};
    } catch (std::exception& e) {
        return std::unexpected(std::string("Start failed: ") + e.what());
    }
}

void UdpSocket::stop() {
    try {
        impl_->io_ctx.stop();
        if (impl_->io_thread.joinable())
            impl_->io_thread.join();
    } catch (...) {}
}

uint16_t UdpSocket::port() const { return impl_->port; }

UdpSocket::~UdpSocket() { stop(); }

} // namespace vanguard::net
