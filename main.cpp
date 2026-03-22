#include <SDL3/SDL_main.h>
#include <string>
#include "core/identity.hpp"
#include "storage/messages.hpp"
#include "transport/peer.hpp"
#include "ui/window.hpp"

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) {
        try { port = (uint16_t)std::stoi(argv[1]); }
        catch (...) {}
    }

    std::string suffix = "_" + std::to_string(port);
    const std::filesystem::path identity_path = "vanguard_identity" + suffix + ".bin";
    const std::filesystem::path messages_path = "vanguard_messages" + suffix + ".bin";

    vanguard::Identity identity;
    if (vanguard::Identity::exists(identity_path)) {
        identity = vanguard::Identity::load(identity_path);
    } else {
        identity = vanguard::Identity::generate();
        identity.save(identity_path);
    }

    vanguard::MessageStore store(messages_path);
    IncomingQueue queue;

    vanguard::transport::PeerTransport transport(port);

    transport.on_message([&](vanguard::transport::RawMessage msg) {
        try { queue.push(std::move(msg)); } catch (...) {}
    });

    transport.run();
    vanguard::ui::run_window(identity, store, transport, queue);
    transport.stop();
    return 0;
}
