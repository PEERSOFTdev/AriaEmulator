#ifndef ARIA_FLASH_IMAGE_H
#define ARIA_FLASH_IMAGE_H

#include <cstdint>
#include <string>
#include <vector>

#include "fat12.h"
#include "text_codec.h"

// The contents of drive B:, the Aria internal flash (HANDOFF 1). The user
// chooses which firmware build to run, because the Czech builds differ in
// encoding: 6.87 is Kamenicky like the Eureka A4, 6.99 and 7.05 are CP852.
//
// Accepted sources, all exactly as Robotron distributed them:
//   - a card update ZIP (CARD\CC0705.ZIP) holding FLASHDSK.IMG,
//   - a serial update ZIP (SERIAL\CSR0687.ZIP) holding ARIAA4.1-3,
//   - a bare FLASHDSK.IMG,
//   - any one of ARIAA4.1-3 (its siblings are read from the same folder),
//   - a folder holding either of the two.
enum class FlashSource { kCardImage, kSerialParts };

struct FlashImage {
  std::vector<uint8_t> bytes;  // Volume image; serial builds span all 2 MB.
  FlashSource source = FlashSource::kCardImage;
  std::wstring origin;         // The path the image was loaded from.
  std::string label;           // Raw volume label, e.g. "Build  705\xFB".
  std::wstring version;        // "7.05", parsed from the label; may be empty.
  TextEncoding detected = TextEncoding::kUnknown;
  TextEncoding encoding = TextEncoding::kUnknown;  // Effective one.
  bool encoding_forced = false;
};

// `forced` overrides detection unless it is kUnknown. On failure returns
// false with a Czech message for the user in `error`.
bool LoadFlashImage(const std::wstring& path, TextEncoding forced,
                    FlashImage& image, std::wstring& error);

// Joins serial update parts (each a 256-byte "ARIA" header plus payload),
// checking magic, numbering, length, CRC-32 and that the result is the
// whole 2 MB flash (HANDOFF 1). Exposed for the self-test.
bool JoinSerialParts(const std::vector<std::vector<uint8_t>>& parts,
                     std::vector<uint8_t>& flash, std::wstring& error);

// Decides between Kamenicky and CP852 from SYSTEM\MESSAGES.DAT, the Czech
// system messages. kUnknown for English or Thai builds and for anything
// too close to call.
TextEncoding DetectEncoding(const Fat12Volume& volume);
TextEncoding DetectEncodingOfText(const std::vector<uint8_t>& text);

#endif
