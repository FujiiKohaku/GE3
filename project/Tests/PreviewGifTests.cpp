#include "ModelPreview/PreviewGif.h"
#include <iostream>
#include <stdexcept>

int main()
{
    try {
        std::filesystem::create_directories("generated/preview-gif-tests");
        PreviewGif gif;
        bool rejected = false;
        try { gif.Begin("generated/preview-gif-tests/invalid.gif", 0, 1); }
        catch (const std::exception&) { rejected = true; }
        if (!rejected) { throw std::runtime_error("Zero-sized GIF was accepted"); }
        gif.Begin("generated/preview-gif-tests/patterns.gif", 193, 65);
        std::vector<std::uint8_t> frame(193 * 65);
        std::ofstream expected("generated/preview-gif-tests/expected.rgb", std::ios::binary);
        unsigned int random = 731;
        for (int pattern = 0; pattern < 3; ++pattern) {
            for (std::size_t index = 0; index < frame.size(); ++index) {
                unsigned int value = 0;
                if (pattern == 1) { value = static_cast<unsigned int>(index % 252); }
                if (pattern == 2) {
                    random = random * 1664525u + 1013904223u;
                    value = (random >> 16) % 252;
                }
                frame[index] = static_cast<std::uint8_t>(value);
                expected.put(static_cast<char>((value / 42) * 255 / 5));
                expected.put(static_cast<char>((value / 6 % 7) * 255 / 6));
                expected.put(static_cast<char>((value % 6) * 255 / 5));
            }
            gif.AddFrame(frame, 7);
        }
        rejected = false;
        try { gif.AddFrame(std::vector<std::uint8_t>(1), 7); }
        catch (const std::exception&) { rejected = true; }
        if (!rejected) { throw std::runtime_error("Wrong-sized frame was accepted"); }
        gif.Finish();
        gif.Finish();
        // Reuse a writer, including the one-pixel and partial-byte cases.
        gif.Begin("generated/preview-gif-tests/single.gif", 1, 1);
        gif.AddFrame(std::vector<std::uint8_t>(1, 251), 10);
        gif.Finish();
        std::cout << "PASS: GIF writer, dimensions, reuse, long runs, repeated dictionary resets\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
