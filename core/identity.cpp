#include "identity.hpp"
#include <sodium.h>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace vanguard::core {

Result<Identity> Identity::generate() {
    if (sodium_init() < 0)
        return std::unexpected("libsodium init failed");

    Identity id;
    crypto_box_keypair(id.public_key.data(), id.secret_key.data());
    return id;
}

Result<void> Identity::save(const std::filesystem::path& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return std::unexpected("Cannot open file for writing: " + path.string());

    f.write(reinterpret_cast<const char*>(public_key.data()), KEY_SIZE);
    f.write(reinterpret_cast<const char*>(secret_key.data()), KEY_SIZE);
    return {};
}

Result<Identity> Identity::load(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::unexpected("Cannot open file for reading: " + path.string());

    Identity id;
    f.read(reinterpret_cast<char*>(id.public_key.data()), KEY_SIZE);
    f.read(reinterpret_cast<char*>(id.secret_key.data()), KEY_SIZE);

    if (!f) return std::unexpected("Failed to read identity from: " + path.string());
    return id;
}

bool Identity::exists(const std::filesystem::path& path) {
    return std::filesystem::exists(path);
}

std::string Identity::id() const {
    std::ostringstream oss;
    for (auto byte : public_key)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    return oss.str();
}

} // namespace vanguard::core
