#include "Utf8Utility.h"

namespace {
constexpr uint32_t kReplacementCharacter = 0xFFFD;
}

std::vector<uint32_t> Utf8Utility::Decode(const std::string& text)
{
    std::vector<uint32_t> codepoints;
    codepoints.reserve(text.size());

    size_t index = 0;
    while (index < text.size()) {
        const uint8_t first = static_cast<uint8_t>(text[index]);
        uint32_t codepoint = kReplacementCharacter;
        size_t sequenceLength = 1;

        if ((first & 0x80u) == 0) {
            codepoint = first;
        } else if ((first & 0xE0u) == 0xC0u) {
            sequenceLength = 2;
            codepoint = first & 0x1Fu;
        } else if ((first & 0xF0u) == 0xE0u) {
            sequenceLength = 3;
            codepoint = first & 0x0Fu;
        } else if ((first & 0xF8u) == 0xF0u) {
            sequenceLength = 4;
            codepoint = first & 0x07u;
        }

        bool isValid = index + sequenceLength <= text.size();
        if (sequenceLength > 1 && isValid) {
            for (size_t offset = 1; offset < sequenceLength; ++offset) {
                const uint8_t continuation = static_cast<uint8_t>(text[index + offset]);
                if ((continuation & 0xC0u) != 0x80u) {
                    isValid = false;
                    break;
                }
                codepoint = (codepoint << 6u) | (continuation & 0x3Fu);
            }
        }

        if (!isValid) {
            codepoint = kReplacementCharacter;
            sequenceLength = 1;
        }

        if (sequenceLength == 2 && codepoint < 0x80u) {
            codepoint = kReplacementCharacter;
        }
        if (sequenceLength == 3 && codepoint < 0x800u) {
            codepoint = kReplacementCharacter;
        }
        if (sequenceLength == 4 && codepoint < 0x10000u) {
            codepoint = kReplacementCharacter;
        }
        if (codepoint > 0x10FFFFu) {
            codepoint = kReplacementCharacter;
        }
        if (codepoint >= 0xD800u && codepoint <= 0xDFFFu) {
            codepoint = kReplacementCharacter;
        }

        codepoints.push_back(codepoint);
        index += sequenceLength;
    }

    return codepoints;
}
