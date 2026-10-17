#ifndef ARIA_CARD_FOLDER_H
#define ARIA_CARD_FOLDER_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "fat12.h"
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

  // An empty card of `size` bytes, formatted as Load formats one, with
  // `label` made a volume label by the Aria's rules: what a new image card
  // starts as.
  static std::vector<uint8_t> Formatted(uint32_t size, const std::wstring& label, TextEncoding encoding);
  // An empty diskette for the Backpack drive (HANDOFF 6.7), as DOS FORMAT
  // lays one out, so other PCs and tools read it too: 1.44 MB with media
  // F0h, 18 sectors a track, 224 root entries, one sector a cluster; 720 KB
  // with F9h, 9 a track, 112 entries, two a cluster.
  static std::vector<uint8_t> FormattedDiskette(bool high_density, const std::wstring& label,
                                                TextEncoding encoding);

  // The card sizes `folder` fits on, smallest first, and the bytes its
  // files take. False with a Czech message when it cannot be read.
  static bool FittingSizes(const std::filesystem::path& folder, TextEncoding encoding, std::vector<uint32_t>& sizes,
                           uint64_t& bytes, std::wstring& error);

  // Reads `folder` into a `size`-byte card image. On failure returns false
  // with a Czech message; when the folder is simply too big, `needed` gets
  // the smallest card size that would take it (0 when none would).
  bool Load(const std::filesystem::path& folder, uint32_t size, TextEncoding encoding, std::vector<uint8_t>& image,
            std::wstring& error, uint32_t* needed = nullptr);
  // The same for a Backpack diskette (HANDOFF 6.7): `folder` as a 1.44 MB
  // diskette laid out as FormattedDiskette lays one, the label from the
  // folder's name. Writing back is Save, whatever the Aria made of it --
  // its FORMAT may turn it into 720 KB, Save reads the geometry anew.
  bool LoadDiskette(const std::filesystem::path& folder, TextEncoding encoding, std::vector<uint8_t>& image,
                    std::wstring& error);
  // Writes `image` back into the home folder Load read. A file the card
  // holds but cannot be read (a broken cluster chain) does not stop the
  // rest: its folder copy is left alone, never trashed, and its card path
  // goes to `damaged`. False only when nothing could be done.
  bool Save(const std::vector<uint8_t>& image, std::wstring& error, std::vector<std::wstring>* damaged = nullptr);

  const std::filesystem::path& home() const { return home_; }

 private:
  // Load's work for any layout: false with `too_big` when the folder does
  // not fit `format`, the error then left to the caller.
  bool LoadAs(const std::filesystem::path& folder, const Fat12Format& format, TextEncoding encoding,
              std::vector<uint8_t>& image, std::wstring& error, bool& too_big);

  struct Imported {
    std::filesystem::path host;
    bool directory = false;
    uint64_t hash = 0;  // Of the contents, for files.
    uint8_t attributes = 0;  // DOS read-only, hidden, system, archive.
  };

  std::filesystem::path home_;
  TextEncoding encoding_ = TextEncoding::kUnknown;
  // Card path ("DIR\\NAME.EXT", bytes in the card's encoding) -> host.
  std::map<std::string, Imported> imported_;
};

#endif
