#pragma once
#include <array>
#include <string>
#include <filesystem>

namespace vanguard {

constexpr size_t PUBLIC_KEY_SIZE = 32;
constexpr size_t SECRET_KEY_SIZE = 32;

struct Identity {
    std::array<unsigned char, PUBLIC_KEY_SIZE> public_key;
    std::array<unsigned char, SECRET_KEY_SIZE> secret_key;

    // Генерация нового ID (первый запуск)
    static Identity generate();

    // Сохранить Identity в файл
    void save(const std::filesystem::path& path) const;

    // Загрузить Identity из файла
    static Identity load(const std::filesystem::path& path);

    // Проверить существует ли сохранённый Identity
    static bool exists(const std::filesystem::path& path);

    // Человекочитаемый ID (hex публичного ключа)
    std::string id() const;
};

} // namespace vanguard
