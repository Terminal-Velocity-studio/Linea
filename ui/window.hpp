#pragma once
#include <mutex>
#include <queue>
#include <vector>
#include "core/identity.hpp"
#include "storage/messages.hpp"
#include "transport/peer.hpp"

// Поток-безопасная очередь входящих сообщений
struct IncomingQueue {
    std::mutex mtx;
    std::queue<vanguard::transport::RawMessage> messages;

    void push(vanguard::transport::RawMessage msg) {
        std::lock_guard<std::mutex> lock(mtx);
        messages.push(std::move(msg));
    }

    bool pop(vanguard::transport::RawMessage& out) {
        std::lock_guard<std::mutex> lock(mtx);
        if (messages.empty()) return false;
        out = std::move(messages.front());
        messages.pop();
        return true;
    }
};

namespace vanguard::ui {
    void run_window(
        vanguard::Identity& identity,
        vanguard::MessageStore& store,
        vanguard::transport::PeerTransport& transport,
        IncomingQueue& queue
    );
}
