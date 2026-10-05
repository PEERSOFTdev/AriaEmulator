#ifndef ARIA_TEXT_CODEC_H
#define ARIA_TEXT_CODEC_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Czech Aria firmware exists in two 8-bit encodings (HANDOFF 1): build 6.87
// (serial CSR0687) uses Kamenicky / KEYBCS2, the same as the Eureka A4,
// while 6.99 and 7.05 use CP852 / Latin 2. English builds are plain ASCII.
enum class TextEncoding { kUnknown, kKamenicky, kCp852 };

std::vector<uint8_t> Encode(TextEncoding encoding, std::wstring_view text);
std::wstring Decode(TextEncoding encoding, const uint8_t* bytes,
                    std::size_t length);

// Stable short names, used on the command line and in settings.
const wchar_t* EncodingName(TextEncoding encoding);
bool ParseEncodingName(std::wstring_view name, TextEncoding& encoding);

#endif
