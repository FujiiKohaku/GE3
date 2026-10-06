#include "PreviewGif.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace {
void AppendCode(std::vector<std::uint8_t>& bytes, unsigned int code,
    unsigned int& bits, unsigned int& bitCount)
{
    bits |= code << bitCount;
    bitCount += 9;
    while (bitCount >= 8) {
        bytes.push_back(static_cast<std::uint8_t>(bits & 255));
        bits >>= 8;
        bitCount -= 8;
    }
}
}

void PreviewGif::Byte(std::uint8_t value)
{
    stream_.put(static_cast<char>(value));
}

void PreviewGif::Word(std::uint16_t value)
{
    Byte(static_cast<std::uint8_t>(value & 255));
    Byte(static_cast<std::uint8_t>(value >> 8));
}

void PreviewGif::Begin(const std::filesystem::path& path, std::uint16_t width, std::uint16_t height)
{
    if (stream_.is_open() || width == 0 || height == 0) {
        throw std::runtime_error("Invalid GIF recording state or dimensions");
    }
    stream_.clear();
    stream_.open(path, std::ios::binary | std::ios::trunc);
    if (!stream_) {
        throw std::runtime_error("Could not create GIF file");
    }
    width_ = width;
    height_ = height;
    stream_.write("GIF89a", 6);
    Word(width);
    Word(height);
    Byte(0xf7); // 256-entry global color table.
    Byte(0);
    Byte(0);
    for (int index = 0; index < 252; ++index) {
        Byte(static_cast<std::uint8_t>((index / 42) * 255 / 5));
        Byte(static_cast<std::uint8_t>((index / 6 % 7) * 255 / 6));
        Byte(static_cast<std::uint8_t>((index % 6) * 255 / 5));
    }
    for (int gray = 0; gray < 4; ++gray) {
        for (int channel = 0; channel < 3; ++channel) {
            Byte(static_cast<std::uint8_t>(gray * 85));
        }
    }
    Byte(0x21);
    Byte(0xff);
    Byte(11);
    stream_.write("NETSCAPE2.0", 11);
    Byte(3);
    Byte(1);
    Word(0); // Infinite loop.
    Byte(0);
    if (!stream_) {
        throw std::runtime_error("Could not write GIF header");
    }
}

std::uint8_t PreviewGif::PaletteIndex(int red, int green, int blue)
{
    red = std::clamp(red, 0, 255);
    green = std::clamp(green, 0, 255);
    blue = std::clamp(blue, 0, 255);
    int r = (red * 5 + 127) / 255;
    int g = (green * 6 + 127) / 255;
    int b = (blue * 5 + 127) / 255;
    return static_cast<std::uint8_t>(r * 42 + g * 6 + b);
}

void PreviewGif::AddFrame(const std::vector<std::uint8_t>& indices,
    std::uint16_t delayCentiseconds)
{
    if (!stream_.is_open() || indices.size() != static_cast<std::size_t>(width_) * height_) {
        throw std::runtime_error("Invalid GIF frame dimensions");
    }
    Byte(0x21);
    Byte(0xf9);
    Byte(4);
    Byte(4); // Keep previous full-frame image until the next one.
    Word(delayCentiseconds);
    Byte(0);
    Byte(0);
    Byte(0x2c);
    Word(0);
    Word(0);
    Word(width_);
    Word(height_);
    Byte(0);
    Byte(8); // Minimum LZW code size.

    std::vector<std::uint8_t> bytes;
    bytes.reserve(indices.size() * 2);
    std::unordered_map<unsigned int, unsigned int> dictionary;
    dictionary.reserve(256);
    unsigned int bits = 0;
    unsigned int bitCount = 0;
    unsigned int nextCode = 258;
    AppendCode(bytes, 256, bits, bitCount);
    unsigned int prefix = indices.front();
    for (std::size_t index = 1; index < indices.size(); ++index) {
        unsigned int symbol = indices[index];
        unsigned int key = prefix * 256 + symbol;
        auto entry = dictionary.find(key);
        if (entry != dictionary.end()) {
            prefix = entry->second;
            continue;
        }
        AppendCode(bytes, prefix, bits, bitCount);
        if (nextCode < 500) {
            dictionary.emplace(key, nextCode);
            ++nextCode;
        } else {
            // Reset before the decoder would need ten-bit codes.
            AppendCode(bytes, 256, bits, bitCount);
            dictionary.clear();
            nextCode = 258;
        }
        prefix = symbol;
    }
    AppendCode(bytes, prefix, bits, bitCount);
    AppendCode(bytes, 257, bits, bitCount);
    if (bitCount > 0) {
        bytes.push_back(static_cast<std::uint8_t>(bits & 255));
    }
    for (std::size_t offset = 0; offset < bytes.size(); offset += 255) {
        std::size_t count = (std::min)(std::size_t(255), bytes.size() - offset);
        Byte(static_cast<std::uint8_t>(count));
        stream_.write(reinterpret_cast<const char*>(bytes.data() + offset), count);
    }
    Byte(0);
    if (!stream_) {
        throw std::runtime_error("Could not write GIF frame (check disk space)");
    }
}

void PreviewGif::Finish()
{
    if (!stream_.is_open()) {
        return;
    }
    Byte(0x3b);
    stream_.flush();
    bool succeeded = static_cast<bool>(stream_);
    stream_.close();
    if (!succeeded) {
        throw std::runtime_error("Could not finish GIF file");
    }
}

void PreviewGif::Abort() noexcept
{
    stream_.close();
}
