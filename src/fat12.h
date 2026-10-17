#ifndef ARIA_FAT12_H
#define ARIA_FAT12_H

#include <cstdint>
#include <string>
#include <vector>

// Read-only view of the FAT12 volume on the Aria internal flash (drive B:).
// The boot record is Datalight "DLRDISK" with one FAT and 16 root entries
// (HANDOFF 1), so generic tools that expect two FATs reject it; this reader
// takes every geometry value from the BPB instead of assuming one.
struct FatEntry {
  std::string name;   // "NAME.EXT", upper case as stored.
  uint8_t attributes = 0;
  uint16_t first_cluster = 0;
  uint32_t size = 0;
  uint16_t time = 0;  // DOS format, as in the directory entry.
  uint16_t date = 0;
  bool IsDirectory() const { return (attributes & 0x10) != 0; }
};

// An empty FAT12 volume: 512-byte sectors, one reserved sector, two FATs,
// media F8h, 55AAh signature, and the volume label in the root directory
// when one is given. Sectors per cluster is the smallest power of two that
// keeps FAT12 under 4085 clusters.
struct Fat12Format {
  uint32_t sectors = 0;
  uint16_t root_entries = 112;
  uint16_t sectors_per_track = 0;  // BPB geometry; 0 for media without one.
  uint16_t heads = 0;
  // A diskette as DOS FORMAT lays it out: its media byte (F0h 1.44 MB,
  // F9h 720 KB) and cluster size; 0 picks the smallest that keeps FAT12.
  uint8_t media = 0xF8;
  uint8_t sectors_per_cluster = 0;
  std::string oem = "ARIAEMU ";    // 8 characters.
  std::string label;               // Up to 11 characters, in the volume's encoding.
};
std::vector<uint8_t> FormatFat12(const Fat12Format& format);

class Fat12Volume {
 public:
  // `image` must outlive the volume.
  bool Open(const std::vector<uint8_t>& image, std::wstring& error);

  const std::string& Label() const { return label_; }
  // Entries of a directory given as "" (root) or "SYSTEM\\SOUNDS".
  bool List(const std::string& directory, std::vector<FatEntry>& entries) const;
  // File given as "SYSTEM\\MESSAGES.DAT"; case-insensitive.
  bool ReadFile(const std::string& path, std::vector<uint8_t>& data) const;

 private:
  bool ReadChain(uint16_t cluster, uint32_t limit, std::vector<uint8_t>& data) const;
  void ParseDirectory(const uint8_t* bytes, std::size_t count,
                      std::vector<FatEntry>& entries) const;
  bool Find(const std::string& path, FatEntry& entry) const;
  uint16_t NextCluster(uint16_t cluster) const;

  const std::vector<uint8_t>* image_ = nullptr;
  uint32_t bytes_per_sector_ = 0;
  uint32_t sectors_per_cluster_ = 0;
  uint32_t fat_offset_ = 0;
  uint32_t root_offset_ = 0;
  uint32_t root_entries_ = 0;
  uint32_t data_offset_ = 0;
  uint32_t cluster_count_ = 0;
  std::string label_;
};

#endif
