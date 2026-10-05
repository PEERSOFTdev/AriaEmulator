#include "flash_image.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "zip_reader.h"

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kSerialHeader = 256;
// The headers carry no part count, so a missing last part would join
// cleanly. Every serial update spans the whole flash (HANDOFF 1), which
// makes the total the check.
constexpr std::size_t kFlashSize = 2 * 1024 * 1024;

uint16_t Read16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
uint32_t Read32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

std::string Upper(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return text;
}

bool ReadWholeFile(const fs::path& path, std::vector<uint8_t>& data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return !file.bad();
}

bool IsSerialPartName(const std::string& upper_name) {
  return upper_name.size() == 8 && upper_name.compare(0, 7, "ARIAA4.") == 0 &&
         std::isdigit(static_cast<unsigned char>(upper_name[7]));
}

// Serial parts from a folder: ARIAA4.1, ARIAA4.2, ... until one is missing.
bool ReadSerialFolder(const fs::path& folder, std::vector<std::vector<uint8_t>>& parts) {
  parts.clear();
  for (int number = 1; number <= 9; ++number) {
    std::vector<uint8_t> data;
    if (!ReadWholeFile(folder / (L"ARIAA4." + std::to_wstring(number)), data)) break;
    parts.push_back(std::move(data));
  }
  return !parts.empty();
}

// Version from the 11-byte volume label "Build  705" + check mark 0xFB
// (HANDOFF 1): three digits, the first being the major number.
std::wstring VersionFromLabel(const std::string& label) {
  for (std::size_t index = 0; index + 2 < label.size(); ++index) {
    const auto digit = [&](std::size_t at) {
      return std::isdigit(static_cast<unsigned char>(label[at])) != 0;
    };
    if (digit(index) && digit(index + 1) && digit(index + 2)) {
      return std::wstring{static_cast<wchar_t>(label[index]), L'.',
                          static_cast<wchar_t>(label[index + 1]),
                          static_cast<wchar_t>(label[index + 2])};
    }
  }
  return {};
}

bool Finish(std::vector<uint8_t> bytes, FlashSource source, const std::wstring& origin,
            TextEncoding forced, FlashImage& image, std::wstring& error) {
  image = FlashImage{};
  image.bytes = std::move(bytes);
  image.source = source;
  image.origin = origin;
  Fat12Volume volume;
  if (!volume.Open(image.bytes, error)) return false;
  // CONFIG.SYS in the root is what the boot ROM reads first (HANDOFF 2);
  // without it the image may be FAT12 but is not an Aria drive B:.
  std::vector<uint8_t> config;
  if (!volume.ReadFile("CONFIG.SYS", config)) {
    error = L"Na disku chybí CONFIG.SYS, nejde o disk B: Arie.";
    return false;
  }
  image.label = volume.Label();
  image.version = VersionFromLabel(image.label);
  image.detected = DetectEncoding(volume);
  if (forced != TextEncoding::kUnknown) {
    image.encoding = forced;
    image.encoding_forced = true;
  } else {
    // English and Thai builds carry no Czech text to tell from. CP852 is
    // the encoding of the newer builds, so it is the less surprising guess.
    image.encoding = image.detected != TextEncoding::kUnknown ? image.detected
                                                              : TextEncoding::kCp852;
  }
  return true;
}

}  // namespace

TextEncoding DetectEncodingOfText(const std::vector<uint8_t>& text) {
  // The same Czech letters in each encoding: c, e, r, s, z with caron,
  // u with ring, y with acute. The byte values do not collide between the
  // two lists. On the wrong text a list scores only a little (CP852 0x9F
  // is also Kamenicky t with caron), hence a ratio rather than a match.
  static constexpr uint8_t kKamenicky[] = {0x87, 0x88, 0xA9, 0xA8, 0x91, 0x96, 0x98};
  static constexpr uint8_t kCp852[] = {0x9F, 0xD8, 0xFD, 0xE7, 0xA7, 0x85, 0xEC};
  std::size_t kamenicky = 0, cp852 = 0;
  for (uint8_t byte : text) {
    if (std::find(std::begin(kKamenicky), std::end(kKamenicky), byte) != std::end(kKamenicky)) ++kamenicky;
    if (std::find(std::begin(kCp852), std::end(kCp852), byte) != std::end(kCp852)) ++cp852;
  }
  // Measured on MESSAGES.DAT (HANDOFF 1): 6.87 scores 1456 to 64, 6.99
  // 111 to 1387, 7.05 122 to 1408. A factor of four with a floor of 100
  // leaves a wide margin on both sides.
  if (kamenicky >= 100 && kamenicky >= 4 * cp852) return TextEncoding::kKamenicky;
  if (cp852 >= 100 && cp852 >= 4 * kamenicky) return TextEncoding::kCp852;
  return TextEncoding::kUnknown;
}

TextEncoding DetectEncoding(const Fat12Volume& volume) {
  std::vector<uint8_t> messages;
  if (!volume.ReadFile("SYSTEM\\MESSAGES.DAT", messages)) return TextEncoding::kUnknown;
  return DetectEncodingOfText(messages);
}

bool JoinSerialParts(const std::vector<std::vector<uint8_t>>& parts,
                     std::vector<uint8_t>& flash, std::wstring& error) {
  flash.clear();
  std::vector<const std::vector<uint8_t>*> ordered(parts.size(), nullptr);
  for (const auto& part : parts) {
    if (part.size() < kSerialHeader || part[0] != 'A' || part[1] != 'R' || part[2] != 'I' ||
        part[3] != 'A') {
      error = L"Díl sériové aktualizace nemá hlavičku ARIA.";
      return false;
    }
    const uint16_t number = Read16(part.data() + 4);
    if (number < 1 || number > parts.size() || ordered[number - 1] != nullptr) {
      error = L"Díly sériové aktualizace nejsou úplné nebo se opakují.";
      return false;
    }
    const uint32_t length = Read32(part.data() + 6);
    if (part.size() - kSerialHeader != length) {
      error = L"Díl " + std::to_wstring(number) + L" sériové aktualizace má jinou délku, než udává hlavička.";
      return false;
    }
    if (Crc32(part.data() + kSerialHeader, length) != Read32(part.data() + 10)) {
      error = L"Díl " + std::to_wstring(number) + L" sériové aktualizace má chybný kontrolní součet.";
      return false;
    }
    ordered[number - 1] = &part;
  }
  for (const auto* part : ordered) {
    flash.insert(flash.end(), part->begin() + kSerialHeader, part->end());
  }
  if (flash.size() != kFlashSize) {
    error = L"Díly sériové aktualizace nedávají celou flash 2 MB, některý díl chybí.";
    flash.clear();
    return false;
  }
  return true;
}

bool LoadFlashImage(const std::wstring& path_text, TextEncoding forced,
                    FlashImage& image, std::wstring& error) {
  const fs::path path(path_text);
  std::error_code ignored;
  std::vector<uint8_t> bytes;

  if (fs::is_directory(path, ignored)) {
    if (ReadWholeFile(path / L"FLASHDSK.IMG", bytes)) {
      return Finish(std::move(bytes), FlashSource::kCardImage, path_text, forced, image, error);
    }
    std::vector<std::vector<uint8_t>> parts;
    if (ReadSerialFolder(path, parts)) {
      if (!JoinSerialParts(parts, bytes, error)) return false;
      return Finish(std::move(bytes), FlashSource::kSerialParts, path_text, forced, image, error);
    }
    error = L"Ve složce není FLASHDSK.IMG ani díly ARIAA4.1 až ARIAA4.3.";
    return false;
  }

  if (IsSerialPartName(Upper(path.filename().string()))) {
    std::vector<std::vector<uint8_t>> parts;
    if (!ReadSerialFolder(path.parent_path(), parts)) {
      error = L"Díly sériové aktualizace se nepodařilo přečíst.";
      return false;
    }
    if (!JoinSerialParts(parts, bytes, error)) return false;
    return Finish(std::move(bytes), FlashSource::kSerialParts, path_text, forced, image, error);
  }

  if (!ReadWholeFile(path, bytes)) {
    error = L"Soubor se nepodařilo otevřít: " + path_text;
    return false;
  }

  if (bytes.size() >= 4 && bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 3 && bytes[3] == 4) {
    std::vector<ZipMember> members;
    if (!ReadZip(bytes, members, error)) return false;
    std::vector<std::vector<uint8_t>> parts;
    for (auto& member : members) {
      const std::string name = Upper(member.name);
      if (name == "FLASHDSK.IMG") {
        return Finish(std::move(member.data), FlashSource::kCardImage, path_text, forced, image, error);
      }
      if (IsSerialPartName(name)) parts.push_back(std::move(member.data));
    }
    if (parts.empty()) {
      error = L"V archivu není FLASHDSK.IMG ani díly ARIAA4.";
      return false;
    }
    if (!JoinSerialParts(parts, bytes, error)) return false;
    return Finish(std::move(bytes), FlashSource::kSerialParts, path_text, forced, image, error);
  }

  return Finish(std::move(bytes), FlashSource::kCardImage, path_text, forced, image, error);
}
