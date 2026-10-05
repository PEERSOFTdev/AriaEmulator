// Drive B: loading without any firmware: a synthetic Datalight-style FAT12
// volume, serial parts and a ZIP built here, plus DEFLATE vectors produced
// by zlib from our own text. Prints PASS on success.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "fat12.h"
#include "flash_image.h"
#include "text_codec.h"
#include "zip_reader.h"

namespace fs = std::filesystem;

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL " << what << "\n";
    ++failures;
  }
}

// zlib raw deflate (level 9, -15 window bits) of four repetitions of a
// three-sentence Czech text without diacritics; 900 bytes, CRC aa3fe969.
constexpr uint8_t kDynamic[] = {
    0xed, 0x8f, 0x41, 0x0e, 0x82, 0x30, 0x14, 0x44, 0xaf, 0x32, 0x07, 0x20, 0x1c, 0x80, 0x1d, 0xc2,
    0xc6, 0x05, 0x09, 0x51, 0x2f, 0xf0, 0x2d, 0x9f, 0xf0, 0x6d, 0x69, 0xc9, 0x6f, 0x6d, 0x22, 0xa7,
    0x37, 0xa0, 0xd1, 0x2b, 0xb8, 0x70, 0x39, 0x79, 0x99, 0xcc, 0xbc, 0x5a, 0x85, 0x70, 0x63, 0x58,
    0x5a, 0x38, 0x7a, 0xc1, 0x12, 0x8c, 0x24, 0x32, 0x58, 0x34, 0xc0, 0x73, 0x96, 0x21, 0xcc, 0x5c,
    0xa2, 0x23, 0x44, 0x8e, 0x09, 0x57, 0x25, 0x71, 0x2e, 0xda, 0x87, 0x99, 0x60, 0x1d, 0x65, 0x8e,
    0x05, 0x66, 0x5e, 0x59, 0xbd, 0xd8, 0x02, 0x21, 0xce, 0x18, 0xef, 0xde, 0x1a, 0x2f, 0x1f, 0x0e,
    0xc2, 0xc2, 0x09, 0xf6, 0xae, 0x6b, 0xd0, 0x90, 0xbf, 0xc5, 0x12, 0x27, 0x36, 0xc8, 0x8f, 0x94,
    0x49, 0x05, 0x3d, 0x27, 0xd6, 0x0b, 0x39, 0x0b, 0x4f, 0x68, 0xcf, 0x7d, 0x89, 0x56, 0xa2, 0xc5,
    0xa1, 0xda, 0xbe, 0x89, 0x4f, 0xdb, 0x02, 0x46, 0x47, 0x71, 0x2a, 0x30, 0x6c, 0xa4, 0xa9, 0x5e,
    0xaf, 0x35, 0x11, 0xfa, 0xa6, 0x6b, 0x8e, 0xf5, 0x1b, 0xb4, 0x3b, 0x38, 0xd5, 0xdd, 0x1e, 0x4b,
    0xd4, 0x7f, 0xc1, 0xbf, 0xe0, 0x6f, 0x0b, 0x3e, 0x01,
};
// Fixed-Huffman block (Z_FIXED) of "Aria Aria Aria Aria braille".
constexpr uint8_t kFixed[] = {
    0x73, 0x2c, 0xca, 0x4c, 0x54, 0x70, 0x44, 0x25, 0x92, 0x8a, 0x12, 0x33, 0x73, 0x72, 0x52, 0x01,
};
// Stored block (level 0) of "stored block".
constexpr uint8_t kStored[] = {
    0x01, 0x0c, 0x00, 0xf3, 0xff, 0x73, 0x74, 0x6f, 0x72, 0x65, 0x64, 0x20, 0x62, 0x6c, 0x6f, 0x63,
    0x6b,
};

void Put16(std::vector<uint8_t>& out, std::size_t at, uint32_t value) {
  out[at] = static_cast<uint8_t>(value);
  out[at + 1] = static_cast<uint8_t>(value >> 8);
}
void Put32(std::vector<uint8_t>& out, std::size_t at, uint32_t value) {
  Put16(out, at, value & 0xFFFF);
  Put16(out, at + 2, value >> 16);
}

void SetFat(std::vector<uint8_t>& image, uint16_t cluster, uint16_t value) {
  const std::size_t at = 512 + cluster * 3u / 2u;
  if (cluster & 1) {
    image[at] = static_cast<uint8_t>((image[at] & 0x0F) | (value << 4));
    image[at + 1] = static_cast<uint8_t>(value >> 4);
  } else {
    image[at] = static_cast<uint8_t>(value);
    image[at + 1] = static_cast<uint8_t>((image[at + 1] & 0xF0) | (value >> 8));
  }
}

void DirEntry(std::vector<uint8_t>& image, std::size_t at, const char* name11, uint8_t attributes,
              uint16_t cluster, uint32_t size) {
  std::memcpy(&image[at], name11, 11);
  image[at + 11] = attributes;
  Put16(image, at + 26, cluster);
  Put32(image, at + 28, size);
}

// The DLRDISK geometry (HANDOFF 1): 1 reserved sector, 1 FAT of 1 sector,
// 16 root entries in sector 2, data from sector 3, one sector per cluster.
std::vector<uint8_t> BuildVolume(const std::vector<uint8_t>& messages) {
  std::vector<uint8_t> image(64 * 512, 0);
  std::memcpy(&image[3], "DLRDISK ", 8);
  Put16(image, 11, 512);
  image[13] = 1;
  Put16(image, 14, 1);
  image[16] = 1;
  Put16(image, 17, 16);
  Put16(image, 19, 64);
  image[21] = 0xF8;
  Put16(image, 22, 1);
  SetFat(image, 0, 0xFF8);
  SetFat(image, 1, 0xFFF);

  const std::string config = "shell=b:\\command.com b:\\ /p /e:512\r\n";
  // The real label form, three digits and a check mark: "Build  705\xFB".
  DirEntry(image, 1024, "Build  999\xFB", 0x08, 0, 0);
  DirEntry(image, 1024 + 32, "CONFIG  SYS", 0x20, 2, static_cast<uint32_t>(config.size()));
  DirEntry(image, 1024 + 64, "SYSTEM     ", 0x10, 3, 0);
  SetFat(image, 2, 0xFFF);
  std::memcpy(&image[3 * 512], config.data(), config.size());
  SetFat(image, 3, 0xFFF);
  // MESSAGES.DAT spans clusters 4, 6 and 5 on purpose: a chain that is not
  // contiguous, so the reader really follows the FAT.
  DirEntry(image, 4 * 512, ".          ", 0x10, 3, 0);
  DirEntry(image, 4 * 512 + 32, "..         ", 0x10, 0, 0);
  DirEntry(image, 4 * 512 + 64, "MESSAGESDAT", 0x20, 4, static_cast<uint32_t>(messages.size()));
  // Cluster n lives in sector n + 1 with this geometry, so the directory
  // (cluster 3) is sector 4 and the data clusters 4, 6, 5 are 5, 7, 6.
  SetFat(image, 4, 6);
  SetFat(image, 6, 5);
  SetFat(image, 5, 0xFFF);
  const uint16_t chain[] = {4, 6, 5};
  for (std::size_t part = 0; part < 3; ++part) {
    const std::size_t from = part * 512;
    if (from >= messages.size()) break;
    const std::size_t count = std::min<std::size_t>(512, messages.size() - from);
    std::memcpy(&image[(chain[part] + 1) * 512], messages.data() + from, count);
  }
  return image;
}

// Serial updates span the whole 2 MB flash, the rest erased to 0xFF.
std::vector<uint8_t> WholeFlash(std::vector<uint8_t> volume) {
  volume.resize(2 * 1024 * 1024, 0xFF);
  return volume;
}

std::vector<uint8_t> SerialPart(uint16_t number, const uint8_t* data, std::size_t length) {
  std::vector<uint8_t> part(256 + length, 0);
  std::memcpy(part.data(), "ARIA", 4);
  Put16(part, 4, number);
  Put32(part, 6, static_cast<uint32_t>(length));
  Put32(part, 10, Crc32(data, length));
  std::memcpy(part.data() + 256, data, length);
  return part;
}

std::vector<uint8_t> StoredZip(const char* name, const std::vector<uint8_t>& data) {
  const std::size_t name_length = std::strlen(name);
  std::vector<uint8_t> zip(30 + name_length, 0);
  Put32(zip, 0, 0x04034b50u);
  Put16(zip, 4, 20);
  Put32(zip, 14, Crc32(data.data(), data.size()));
  Put32(zip, 18, static_cast<uint32_t>(data.size()));
  Put32(zip, 22, static_cast<uint32_t>(data.size()));
  Put16(zip, 26, static_cast<uint32_t>(name_length));
  std::memcpy(&zip[30], name, name_length);
  zip.insert(zip.end(), data.begin(), data.end());
  return zip;
}

void WriteFile(const fs::path& path, const std::vector<uint8_t>& data) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::vector<uint8_t> CzechMessages(TextEncoding encoding) {
  std::wstring text;
  for (int index = 0; index < 20; ++index) {
    text += L"prázdný řádek\tdisk je plný\tpaměť verze %d tečka %d\tžluťoučký kůň\t";
  }
  return Encode(encoding, text);
}

void TestInflate() {
  std::vector<uint8_t> out;
  Check(Inflate(kDynamic, sizeof kDynamic, out), "inflate dynamic");
  Check(out.size() == 900 && Crc32(out.data(), out.size()) == 0xaa3fe969u, "inflate dynamic content");
  out.clear();
  Check(Inflate(kFixed, sizeof kFixed, out), "inflate fixed");
  Check(std::string(out.begin(), out.end()) == "Aria Aria Aria Aria braille", "inflate fixed content");
  out.clear();
  Check(Inflate(kStored, sizeof kStored, out), "inflate stored");
  Check(std::string(out.begin(), out.end()) == "stored block", "inflate stored content");
  out.clear();
  Check(!Inflate(kDynamic, sizeof kDynamic - 20, out), "inflate rejects truncated stream");
}

void TestVolume() {
  const auto messages = CzechMessages(TextEncoding::kKamenicky);
  const auto image = BuildVolume(messages);
  Fat12Volume volume;
  std::wstring error;
  Check(volume.Open(image, error), "fat open");
  Check(volume.Label() == "Build  999\xFB", "fat label");
  std::vector<uint8_t> data;
  Check(volume.ReadFile("config.sys", data) && data.size() > 10 && data[0] == 's', "fat root file");
  Check(volume.ReadFile("SYSTEM\\MESSAGES.DAT", data) && data == messages, "fat chained file");
  std::vector<FatEntry> entries;
  Check(volume.List("SYSTEM", entries) && entries.size() == 1 && entries[0].name == "MESSAGES.DAT",
        "fat list subdirectory");
  Check(!volume.ReadFile("SYSTEM\\NONE.DAT", data), "fat missing file");

  Check(DetectEncoding(volume) == TextEncoding::kKamenicky, "detect kamenicky");
  Check(DetectEncodingOfText(CzechMessages(TextEncoding::kCp852)) == TextEncoding::kCp852, "detect cp852");
  const std::string ascii(4000, 'a');
  Check(DetectEncodingOfText(std::vector<uint8_t>(ascii.begin(), ascii.end())) == TextEncoding::kUnknown,
        "detect ascii as unknown");
}

void TestSerial() {
  const auto image = WholeFlash(BuildVolume(CzechMessages(TextEncoding::kCp852)));
  const std::size_t third = image.size() / 3;
  std::vector<std::vector<uint8_t>> parts = {
      SerialPart(2, image.data() + third, third),
      SerialPart(1, image.data(), third),
      SerialPart(3, image.data() + 2 * third, image.size() - 2 * third),
  };
  std::vector<uint8_t> joined;
  std::wstring error;
  Check(JoinSerialParts(parts, joined, error) && joined == image, "serial join out of order");
  auto corrupt = parts;
  corrupt[0][300] ^= 1;
  Check(!JoinSerialParts(corrupt, joined, error), "serial rejects bad crc");
  auto missing = parts;
  missing.pop_back();
  Check(!JoinSerialParts(missing, joined, error), "serial rejects missing part");
}

void TestLoad() {
  wchar_t temp[MAX_PATH];
  GetTempPathW(MAX_PATH, temp);
  const fs::path root = fs::path(temp) / L"aria-flash-selftest";
  std::error_code ignored;
  fs::remove_all(root, ignored);
  fs::create_directories(root / L"card");
  fs::create_directories(root / L"serial");

  const auto image = BuildVolume(CzechMessages(TextEncoding::kKamenicky));
  WriteFile(root / L"card" / L"FLASHDSK.IMG", image);
  const auto flash = WholeFlash(image);
  const std::size_t half = flash.size() / 2;
  WriteFile(root / L"serial" / L"ARIAA4.1", SerialPart(1, flash.data(), half));
  WriteFile(root / L"serial" / L"ARIAA4.2", SerialPart(2, flash.data() + half, flash.size() - half));
  WriteFile(root / L"card.zip", StoredZip("FLASHDSK.IMG", image));

  FlashImage loaded;
  std::wstring error;
  Check(LoadFlashImage((root / L"card").wstring(), TextEncoding::kUnknown, loaded, error) &&
            loaded.source == FlashSource::kCardImage && loaded.version == L"9.99" &&
            loaded.encoding == TextEncoding::kKamenicky && !loaded.encoding_forced,
        "load card folder");
  Check(LoadFlashImage((root / L"serial" / L"ARIAA4.2").wstring(), TextEncoding::kUnknown, loaded, error) &&
            loaded.source == FlashSource::kSerialParts && loaded.bytes == flash,
        "load serial from second part");
  Check(LoadFlashImage((root / L"card.zip").wstring(), TextEncoding::kCp852, loaded, error) &&
            loaded.encoding == TextEncoding::kCp852 && loaded.encoding_forced &&
            loaded.detected == TextEncoding::kKamenicky,
        "load zip with forced encoding");
  Check(!LoadFlashImage((root / L"nothing.img").wstring(), TextEncoding::kUnknown, loaded, error) &&
            !error.empty(),
        "load missing file reports error");
  fs::remove_all(root, ignored);
}

}  // namespace

int main() {
  TestInflate();
  TestVolume();
  TestSerial();
  TestLoad();
  std::cout << (failures == 0 ? "PASS" : "FAIL") << " flash_selftest\n";
  return failures == 0 ? 0 : 1;
}
