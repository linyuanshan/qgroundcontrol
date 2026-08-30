#include "MarineTask.h"

#include <array>
#include <cstdint>
#include <random>

namespace {

std::string generateUuid()
{
    std::array<std::uint8_t, 16> bytes{};
    std::random_device randomDevice;
    std::uniform_int_distribution<unsigned int> distribution(0, 255);

    for (std::uint8_t& byte : bytes) {
        byte = static_cast<std::uint8_t>(distribution(randomDevice));
    }

    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3F) | 0x80);

    static constexpr char hexDigits[] = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if ((index == 4) || (index == 6) || (index == 8) || (index == 10)) {
            uuid.push_back('-');
        }
        uuid.push_back(hexDigits[bytes[index] >> 4]);
        uuid.push_back(hexDigits[bytes[index] & 0x0F]);
    }

    return uuid;
}

}  // namespace

namespace Marine {

MarineTask::MarineTask() : id(generateUuid()) {}

bool MarineTask::isValid() const
{
    return !id.empty() && (region.outerBoundary.vertices.size() >= 3);
}

}  // namespace Marine
