#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

// Streaming GIF89a writer. One fixed palette prevents frame-to-frame flicker.
// A bounded LZW dictionary keeps codes at nine bits and memory independent of duration.
class PreviewGif {
public:
    void Begin(const std::filesystem::path& path, std::uint16_t width, std::uint16_t height);
    void AddFrame(const std::vector<std::uint8_t>& indices, std::uint16_t delayCentiseconds);
    void Finish();
    void Abort() noexcept;
    static std::uint8_t PaletteIndex(int red, int green, int blue);

private:
    void Byte(std::uint8_t value);
    void Word(std::uint16_t value);
    std::ofstream stream_;
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
};
