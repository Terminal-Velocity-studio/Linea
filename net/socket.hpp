#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <memory>
#include "core/identity.hpp"

namespace vanguard::net {

struct Packet {
    std::string data;
    std::string from_host;
    uint16_t from_port = 0;
};

class UdpSocket {
public:
    // Создать сокет на порту
    static core::Result<std::unique_ptr<UdpSocket>> create(uint16_t port);

    // Отправить пакет
    core::Result<void> send(const std::string& host, uint16_t port, const std::string& data);

    // Callback на входящие пакеты (вызывается из фонового потока)
    void on_packet(std::function<void(Packet)> cb);

    // Запустить приём в фоне
    core::Result<void> start();

    // Остановить
    void stop();

    uint16_t port() const;

    ~UdpSocket();

private:
    UdpSocket() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vanguard::net
