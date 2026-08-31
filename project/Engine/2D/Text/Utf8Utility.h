#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Utf8Utility {
std::vector<uint32_t> Decode(const std::string& text);
}
