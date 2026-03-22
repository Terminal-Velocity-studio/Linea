#pragma once
#include <mutex>
#include <queue>
#include "core/identity.hpp"
#include "storage/message.hpp"
#include "net/peer.hpp"

namespace vanguard::ui {

// Поток-безопасная очередь входящих сообщений
struct IncomingQueue {
    std::mutex mtx;
    std::queue<net::Message> messages;

    void push(net::Message msg) {
        std::lock_guard<std::mutex> lock(mtx);
        messages.push(std::move(msg));
    }

    bool pop(net::Message& out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (messages.empty()) return false;
        out = std::move(messages.front());
        messages.pop();
        return true;
    }
};

// Запустить главное окно
void run(core::Identity& identity,
         storage::MessageStore& store,
         net::Peer& peer,
         IncomingQueue& queue);

} // namespace vanguard::ui
