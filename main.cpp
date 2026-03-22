#include <SDL3/SDL_main.h>
#include <string>
#include <iostream>
#include "core/identity.hpp"
#include "storage/message.hpp"
#include "net/peer.hpp"
#include "ui/window.hpp"
#include <windows.h>
#include <dbghelp.h>

using namespace vanguard;

int main(int argc, char* argv[]) {
    // Порт из аргумента, по умолчанию 8080
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        HANDLE hFile = CreateFileA("crash.dmp", GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = ep;
            mei.ClientPointers = FALSE;
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                hFile, MiniDumpNormal, &mei, nullptr, nullptr);
           CloseHandle(hFile);
       }
        return EXCEPTION_EXECUTE_HANDLER;
    });
    uint16_t port = 8080;
    if (argc > 1) {
        try { port = (uint16_t)std::stoi(argv[1]); }
        catch (...) {
            std::cerr << "Invalid port, using 8080\n";
        }
    }

    std::string suffix = "_" + std::to_string(port);

    // Загружаем или создаём Identity
    auto id_path = "vanguard_identity" + suffix + ".bin";
    core::Identity identity;

    if (core::Identity::exists(id_path)) {
        auto result = core::Identity::load(id_path);
        if (!result) {
            std::cerr << "Failed to load identity: " << result.error() << "\n";
            return 1;
        }
        identity = *result;
    } else {
        auto result = core::Identity::generate();
        if (!result) {
            std::cerr << "Failed to generate identity: " << result.error() << "\n";
            return 1;
        }
        identity = *result;
        auto save = identity.save(id_path);
        if (!save) {
            std::cerr << "Failed to save identity: " << save.error() << "\n";
            return 1;
        }
        std::cout << "New identity created: " << identity.id().substr(0, 16) << "...\n";
    }

    // Загружаем хранилище сообщений
    auto msg_path = "vanguard_messages" + suffix + ".bin";
    storage::MessageStore store(msg_path);

    // Создаём P2P ноду
    auto peer_result = net::Peer::create(port);
    if (!peer_result) {
        std::cerr << "Failed to create peer: " << peer_result.error() << "\n";
        return 1;
    }
    auto& peer = *peer_result;

    // Очередь входящих сообщений
    ui::IncomingQueue queue;

    // Входящие сообщения идут в очередь
    peer->on_message([&](net::Message msg) {
        queue.push(std::move(msg));
    });

    // Запускаем сеть
    auto start = peer->start();
    if (!start) {
        std::cerr << "Failed to start peer: " << start.error() << "\n";
        return 1;
    }

    std::cout << "Vanguard started on port " << port << "\n";
    std::cout << "ID: " << identity.id().substr(0, 16) << "...\n";

    // Запускаем UI
    ui::run(identity, store, *peer, queue);

    peer->stop();
    return 0;
}
