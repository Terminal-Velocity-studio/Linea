#pragma once
#include <array>
#include <string>
#include <filesystem>
#include <expected>

namespace vanguard::core {

constexpr size_t KEY_SIZE = 32;

// Результат операции
template<typename T>
using Result = std::expected<T, std::string>;

struct Identity {
    std::array<unsigned char, KEY_SIZE> public_key;
    std::array<unsigned char, KEY_SIZE> secret_key;

    // Генерация новой пары ключей
    static Result<Identity> generate();

    // Сохранить на диск
    Result<void> save(const std::filesystem::path& path) const;

    // Загрузить с диска
    static Result<Identity> load(const std::filesystem::path& path);

    // Проверить существование файла
    static bool exists(const std::filesystem::path& path);

    // Публичный ключ как hex строка - это ID пользователя
    std::string id() const;
};

} // namespace vanguard::core
