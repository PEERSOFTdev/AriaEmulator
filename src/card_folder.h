#ifndef ARIA_CARD_FOLDER_H
#define ARIA_CARD_FOLDER_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "text_codec.h"

// An SRAM card whose home is a host folder (HANDOFF 6.8, ar-aj1), after the
// Eureka emulator's folder diskettes: the card lives in memory as a FAT12
// image, built from the folder when it goes in and written back into the
// folder when it comes out. The Aria's own drivers see a plain DOS-FAT card
// (FORMAT.EXE+02C3h: a FAT volume from byte 0 of common memory).
//
// Names become upper-case 8.3 in the firmware's encoding (Kamenicky or
// CP852 from FlashImage), so Czech letters the code page has survive; a
// name that does not fit, or clashes, gets ~1, ~2... Writing back keeps
// every file the card still has under the host name it came from, writes
// only what changed, creates what the Aria made under its 8.3 name, and
// moves what the Aria deleted into .aria-trash instead of deleting it.
class FolderCard {
 public:
  // Card sizes the user can choose: powers of two (PcCard mirrors above its
  // size), 64 KB to the 64 MB the window reaches (HANDOFF 6.8).
  static constexpr uint32_t kMinSize = 64u * 1024u;
  static constexpr uint32_t kMaxSize = 64u * 1024u * 1024u;
  // The trash inside the home folder; never put on the card.
  static constexpr const wchar_t* kTrash = L".aria-trash";

  // Reads `folder` into a `size`-byte card image. On failure returns false
  // with a Czech message; when the folder is simply too big, `needed` gets
  // the smallest card size that would take it (0 when none would).
  bool Load(const std::filesystem::path& folder, uint32_t size, TextEncoding encoding, std::vector<uint8_t>& image,
            std::wstring& error, uint32_t* needed = nullptr);
  // Writes `image` back into the home folder Load read.
  bool Save(const std::vector<uint8_t>& image, std::wstring& error);

  const std::filesystem::path& home() const { return home_; }

 private:
  struct Imported {
    std::filesystem::path host;
    bool directory = false;
    uint64_t hash = 0;  // Of the contents, for files.
  };

  std::filesystem::path home_;
  TextEncoding encoding_ = TextEncoding::kUnknown;
  // Card path ("DIR\\NAME.EXT", bytes in the card's encoding) -> host.
  std::map<std::string, Imported> imported_;
};

#endif
