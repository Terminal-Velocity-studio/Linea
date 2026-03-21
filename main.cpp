#include <windows.h>
#include <thread>
#include <string>
#include "core/identity.hpp"
#include "storage/messages.hpp"
#include "transport/peer.hpp"
#include "ui/window.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    uint16_t port = 443;
    if (lpCmdLine && lpCmdLine[0]) {
        try { port = (uint16_t)std::stoi(lpCmdLine); }
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
        queue.push(std::move(msg));
    });

    transport.run();

    vanguard::ui::run_window(identity, store, transport, queue);

    transport.stop();
    return 0;
}
