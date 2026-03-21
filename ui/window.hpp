#pragma once
#include "core/identity.hpp"
#include "storage/messages.hpp"

namespace vanguard::ui {
    // Запускает главное окно приложения
    void run_window(vanguard::Identity& identity, vanguard::MessageStore& store);
}
