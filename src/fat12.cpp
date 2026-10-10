#include "fat12.h"

#include <algorithm>
#include <cctype>

namespace {

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

std::string Trimmed(const uint8_t* bytes, std::size_t length) {
  std::string text(reinterpret_cast<const char*>(bytes), length);
  while (!text.empty() && text.back() == ' ') text.pop_back();
  return text;
}

}  // namespace

std::vector<uint8_t> FormatFat12(const Fat12Format& format) {
  constexpr uint32_t kSector = 512;
  const uint32_t root_sectors = (format.root_entries * 32u + kSector - 1) / kSector;
  uint32_t per_cluster = 1;
  uint32_t fat_sectors = 1;
  for (;;) {
    // FAT size and cluster count depend on each other; a FAT sized for the
    // upper bound of clusters is at most one sector too large.
    const uint32_t data_upper = format.sectors - 1 - root_sectors;
    const uint32_t clusters_upper = data_upper / per_cluster;
    fat_sectors = ((clusters_upper + 2) * 3 / 2 + kSector - 1) / kSector;
    const uint32_t clusters = (format.sectors - 1 - root_sectors - 2 * fat_sectors) / per_cluster;
    if (clusters < 4085 || per_cluster == 128) break;
    per_cluster *= 2;
  }
  std::vector<uint8_t> image(static_cast<std::size_t>(format.sectors) * kSector, 0);
  uint8_t* boot = image.data();
  auto put16 = [&](std::size_t at, uint32_t value) {
    boot[at] = static_cast<uint8_t>(value);
    boot[at + 1] = static_cast<uint8_t>(value >> 8);
  };
  boot[0] = 0xEB;
  boot[1] = 0x3C;
  boot[2] = 0x90;
  for (std::size_t i = 0; i < 8; ++i) boot[3 + i] = i < format.oem.size() ? format.oem[i] : ' ';
  put16(11, kSector);
  boot[13] = static_cast<uint8_t>(per_cluster);
  put16(14, 1);
  boot[16] = 2;
  put16(17, format.root_entries);
  if (format.sectors < 0x10000) {
    put16(19, format.sectors);
  } else {
    put16(32, format.sectors);
    put16(34, format.sectors >> 16);
  }
  boot[21] = 0xF8;
  put16(22, fat_sectors);
  put16(24, format.sectors_per_track);
  put16(26, format.heads);
  boot[510] = 0x55;
  boot[511] = 0xAA;
  for (uint32_t fat = 0; fat < 2; ++fat) {
    uint8_t* table = image.data() + (1 + fat * fat_sectors) * kSector;
    table[0] = 0xF8;
    table[1] = table[2] = 0xFF;
  }
  if (!format.label.empty()) {
    uint8_t* root = image.data() + (1 + 2 * fat_sectors) * kSector;
    for (std::size_t i = 0; i < 11; ++i) root[i] = i < format.label.size() ? format.label[i] : ' ';
    root[11] = 0x08;
  }
  return image;
}

bool Fat12Volume::Open(const std::vector<uint8_t>& image, std::wstring& error) {
  image_ = &image;
  if (image.size() < 512) {
    error = L"Obraz disku je příliš krátký.";
    return false;
  }
  const uint8_t* boot = image.data();
  bytes_per_sector_ = Read16(boot + 11);
  sectors_per_cluster_ = boot[13];
  const uint32_t reserved = Read16(boot + 14);
  const uint32_t fats = boot[16];
  root_entries_ = Read16(boot + 17);
  uint32_t total = Read16(boot + 19);
  if (total == 0) total = Read32(boot + 32);
  const uint32_t sectors_per_fat = Read16(boot + 22);
  if (bytes_per_sector_ != 512 || sectors_per_cluster_ == 0 || fats == 0 ||
      root_entries_ == 0 || sectors_per_fat == 0) {
    error = L"Obraz nemá platný zaváděcí záznam FAT, nejde o disk Arie.";
    return false;
  }
  fat_offset_ = reserved * bytes_per_sector_;
  root_offset_ = fat_offset_ + fats * sectors_per_fat * bytes_per_sector_;
  data_offset_ = root_offset_ + root_entries_ * 32;
  const uint64_t declared = static_cast<uint64_t>(total) * bytes_per_sector_;
  // A serial image is the whole 2 MB flash, longer than the volume it
  // carries; a card image may be cut right after the last used cluster.
  // So only the metadata has to fit, not the declared volume size.
  if (data_offset_ > image.size()) {
    error = L"Obraz disku je zkrácený.";
    return false;
  }
  const uint64_t data_bytes = declared > data_offset_ ? declared - data_offset_ : 0;
  cluster_count_ = static_cast<uint32_t>(data_bytes / (bytes_per_sector_ * sectors_per_cluster_));
  if (cluster_count_ >= 4085) {
    error = L"Obraz není FAT12, nejde o disk Arie.";
    return false;
  }
  label_.clear();
  for (uint32_t index = 0; index < root_entries_; ++index) {
    const uint8_t* entry = boot + root_offset_ + index * 32;
    if (entry[0] == 0) break;
    if (entry[0] != 0xE5 && entry[11] == 0x08) label_ = Trimmed(entry, 11);
  }
  return true;
}

uint16_t Fat12Volume::NextCluster(uint16_t cluster) const {
  const std::size_t offset = fat_offset_ + cluster * 3u / 2u;
  if (offset + 1 >= image_->size()) return 0xFFF;
  const uint16_t pair = Read16(image_->data() + offset);
  return (cluster & 1) ? pair >> 4 : pair & 0x0FFF;
}

bool Fat12Volume::ReadChain(uint16_t cluster, uint32_t limit, std::vector<uint8_t>& data) const {
  data.clear();
  const uint32_t cluster_bytes = bytes_per_sector_ * sectors_per_cluster_;
  uint32_t steps = 0;
  while (cluster >= 2 && cluster < 0xFF0) {
    // Guards against a cyclic chain in a damaged image.
    if (++steps > cluster_count_ + 1 || cluster - 2u >= cluster_count_) return false;
    const std::size_t offset = data_offset_ + static_cast<std::size_t>(cluster - 2) * cluster_bytes;
    if (offset + cluster_bytes > image_->size()) return false;
    data.insert(data.end(), image_->begin() + static_cast<std::ptrdiff_t>(offset),
                image_->begin() + static_cast<std::ptrdiff_t>(offset + cluster_bytes));
    if (data.size() >= limit) break;
    cluster = NextCluster(cluster);
  }
  if (data.size() < limit) return limit == UINT32_MAX;
  data.resize(limit);
  return true;
}

void Fat12Volume::ParseDirectory(const uint8_t* bytes, std::size_t count,
                                 std::vector<FatEntry>& entries) const {
  for (std::size_t index = 0; index < count; ++index) {
    const uint8_t* entry = bytes + index * 32;
    if (entry[0] == 0) break;
    if (entry[0] == 0xE5 || entry[11] == 0x0F || (entry[11] & 0x08) != 0) continue;
    FatEntry item;
    item.name = Trimmed(entry, 8);
    const std::string extension = Trimmed(entry + 8, 3);
    if (!extension.empty()) item.name += "." + extension;
    if (item.name == "." || item.name == "..") continue;
    item.attributes = entry[11];
    item.first_cluster = Read16(entry + 26);
    item.size = Read32(entry + 28);
    item.time = Read16(entry + 22);
    item.date = Read16(entry + 24);
    entries.push_back(item);
  }
}

bool Fat12Volume::List(const std::string& directory, std::vector<FatEntry>& entries) const {
  entries.clear();
  if (directory.empty()) {
    ParseDirectory(image_->data() + root_offset_, root_entries_, entries);
    return true;
  }
  FatEntry entry;
  if (!Find(directory, entry) || !entry.IsDirectory()) return false;
  std::vector<uint8_t> data;
  if (!ReadChain(entry.first_cluster, UINT32_MAX, data)) return false;
  ParseDirectory(data.data(), data.size() / 32, entries);
  return true;
}

bool Fat12Volume::Find(const std::string& path, FatEntry& found) const {
  std::string remaining = Upper(path);
  std::vector<FatEntry> entries;
  ParseDirectory(image_->data() + root_offset_, root_entries_, entries);
  for (;;) {
    const std::size_t slash = remaining.find('\\');
    const std::string part = remaining.substr(0, slash);
    const auto match = std::find_if(entries.begin(), entries.end(),
                                    [&](const FatEntry& entry) { return entry.name == part; });
    if (match == entries.end()) return false;
    if (slash == std::string::npos) {
      found = *match;
      return true;
    }
    if (!match->IsDirectory()) return false;
    std::vector<uint8_t> data;
    if (!ReadChain(match->first_cluster, UINT32_MAX, data)) return false;
    entries.clear();
    ParseDirectory(data.data(), data.size() / 32, entries);
    remaining = remaining.substr(slash + 1);
  }
}

bool Fat12Volume::ReadFile(const std::string& path, std::vector<uint8_t>& data) const {
  FatEntry entry;
  if (!Find(path, entry) || entry.IsDirectory()) return false;
  if (entry.size == 0) {
    data.clear();
    return true;
  }
  return ReadChain(entry.first_cluster, entry.size, data);
}
