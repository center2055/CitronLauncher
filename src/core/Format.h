#pragma once

#include <cstdint>
#include <string>

namespace citron::format {

std::string bytes(std::uint64_t value);
std::string speed(double bytesPerSecond);
std::string eta(double seconds);
std::string percent(double fraction);

}
