// Loads one of Robotron's update archives exactly as distributed and checks
// what HANDOFF 1 records about it: kind, version, encoding and a known
// message. Needs the firmware folder (ARIAFW), which is not in the repo.
//
// Usage: firmware_test.exe FIRMWARE_FOLDER MODE
// Prints PASS on success; exit code 2 when the archive is missing.

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "fat12.h"
#include "flash_image.h"
#include "text_codec.h"

namespace fs = std::filesystem;

namespace {

struct Expected {
  const wchar_t* mode;
  const wchar_t* archive;  // Relative to the firmware folder.
  FlashSource source;
  const wchar_t* version;
  TextEncoding detected;
  const wchar_t* message;  // Must occur in decoded SYSTEM\MESSAGES.DAT.
};

constexpr Expected kModes[] = {
    {L"cc0705", L"CARD\\CC0705.ZIP", FlashSource::kCardImage, L"7.05", TextEncoding::kCp852, L"prázdný řádek"},
    {L"ariav699", L"CARD\\ariav699.zip", FlashSource::kCardImage, L"6.99", TextEncoding::kCp852, L"paměť"},
    {L"csr0687", L"SERIAL\\CSR0687.ZIP", FlashSource::kSerialParts, L"6.87", TextEncoding::kKamenicky, L"prázdný řádek"},
    {L"csr0705", L"SERIAL\\CSR0705.ZIP", FlashSource::kSerialParts, L"7.05", TextEncoding::kCp852, L"paměť"},
    {L"ecrd0704", L"CARD\\ECRD0704.ZIP", FlashSource::kCardImage, L"7.04", TextEncoding::kUnknown, L"yes or no?"},
    {L"eser0704", L"SERIAL\\ESER0704.ZIP", FlashSource::kSerialParts, L"7.04", TextEncoding::kUnknown, L"yes or no?"},
    {L"tc0690", L"CARD\\TC0690.ZIP", FlashSource::kCardImage, L"6.90", TextEncoding::kUnknown, L"yes or no?"},
    {L"tsr0690", L"SERIAL\\TSR0690.ZIP", FlashSource::kSerialParts, L"6.90", TextEncoding::kUnknown, L"yes or no?"},
};

bool Messages(const FlashImage& image, std::vector<uint8_t>& data) {
  Fat12Volume volume;
  std::wstring error;
  return volume.Open(image.bytes, error) && volume.ReadFile("SYSTEM\\MESSAGES.DAT", data);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 3) {
    std::wcout << L"usage: firmware_test FIRMWARE_FOLDER MODE\n";
    return 2;
  }
  const fs::path folder(argv[1]);
  const std::wstring mode = argv[2];
  const Expected* expected = nullptr;
  for (const auto& candidate : kModes) {
    if (mode == candidate.mode) expected = &candidate;
  }
  if (expected == nullptr) {
    std::wcout << L"FAIL unknown mode " << mode << L"\n";
    return 1;
  }
  const fs::path archive = folder / expected->archive;
  std::error_code ignored;
  if (!fs::exists(archive, ignored)) {
    std::wcout << L"SKIP " << mode << L": missing " << archive.wstring() << L"\n";
    return 2;
  }

  FlashImage image;
  std::wstring error;
  bool passed = LoadFlashImage(archive.wstring(), TextEncoding::kUnknown, image, error);
  if (!passed) {
    // Czech message as UTF-8 bytes; wcout would stop at the first
    // non-ASCII character under the C locale.
    std::string utf8;
    for (wchar_t ch : error) utf8 += ch < 0x80 ? static_cast<char>(ch) : '?';
    std::wcout << L"load failed: " << std::wstring(utf8.begin(), utf8.end()) << L"\n";
  } else {
    std::wcout << L"source=" << (image.source == FlashSource::kSerialParts ? L"serial" : L"card")
               << L" version=" << image.version << L" detected=" << EncodingName(image.detected)
               << L" bytes=" << image.bytes.size() << L"\n";
  }
  passed = passed && image.source == expected->source;
  passed = passed && image.version == expected->version;
  passed = passed && image.detected == expected->detected;
  // Serial builds carry the whole 2 MB flash (HANDOFF 1).
  if (expected->source == FlashSource::kSerialParts) passed = passed && image.bytes.size() == 2097152;

  std::vector<uint8_t> messages;
  passed = passed && Messages(image, messages);
  if (passed) {
    const std::wstring text = Decode(image.encoding, messages.data(), messages.size());
    passed = text.find(expected->message) != std::wstring::npos;
  }

  // The serial 7.05 must carry the same system messages as the card 7.05:
  // the two forms are one firmware build (HANDOFF 1).
  if (passed && mode == L"csr0705") {
    FlashImage card;
    std::vector<uint8_t> card_messages;
    passed = LoadFlashImage((folder / L"CARD\\CC0705.ZIP").wstring(), TextEncoding::kUnknown, card, error) &&
             Messages(card, card_messages) && card_messages == messages;
  }

  std::wcout << (passed ? L"PASS " : L"FAIL ") << mode << L"\n";
  return passed ? 0 : 1;
}
