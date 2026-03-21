#include "identity.hpp"
#include <sodium.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>

namespace vanguard {

// Генерация новой пары ключей через libsodium
Identity Identity::generate() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium init failed");
    }
    Identity id;
    crypto_box_keypair(id.public_key.data(), id.secret_key.data());
    return id;
}

// Сохраняем оба ключа в бинарный файл
void Identity::save(const std::filesystem::path& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot save identity");
    f.write(reinterpret_cast<const char*>(public_key.data()), PUBLIC_KEY_SIZE);
    f.write(reinterpret_cast<const char*>(secret_key.data()), SECRET_KEY_SIZE);
}

// Загружаем ключи из файла
Identity Identity::load(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot load identity");
    Identity id;
    f.read(reinterpret_cast<char*>(id.public_key.data()), PUBLIC_KEY_SIZE);
    f.read(reinterpret_cast<char*>(id.secret_key.data()), SECRET_KEY_SIZE);
    return id;
}

// Проверяем есть ли уже сохранённый файл
bool Identity::exists(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

// Публичный ключ в hex строку - это и есть твой адрес в сети
std::string Identity::id() const {
    std::ostringstream oss;
    for (auto byte : public_key) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return oss.str();
}

} // namespace vanguard
