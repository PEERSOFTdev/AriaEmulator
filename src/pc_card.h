#ifndef ARIA_PC_CARD_H
#define ARIA_PC_CARD_H

#include <cstdint>
#include <vector>

// A memory card in the Aria's PCMCIA slot, drive C: (HANDOFF 6.8). The
// board shows it through the D000h window in mode 3; the original drivers
// from drive B: (DLFFSA.EXE, TRICKSS2.EXE, FORMAT.EXE) do everything else.
//
// Only SRAM cards so far (bead ar-3ap): plain read/write memory kept by the
// card's own battery. Flash cards come with ar-ibv.
class PcCard {
 public:
  // An SRAM card of `size` bytes, which must be a power of two of at least
  // 64 KB: the card mirrors its memory above its size, as an address
  // decoder with fewer lines does, and FORMAT.EXE finds the size by that
  // wrap in 64 KB steps (FORMAT.EXE+25D0h). Shorter `contents` is padded.
  static PcCard Sram(uint32_t size, std::vector<uint8_t> contents = {});

  // REG# low (0480h bit 7 = 0) selects attribute memory, where a card keeps
  // its CIS (FORMAT.EXE+1DA9h). SRAM cards here have none: attribute space
  // reads FFh, an empty tuple chain, and ignores writes. FORMAT.EXE takes a
  // card for SRAM by writing to common memory before it looks at the CIS
  // (FORMAT.EXE+2954h).
  uint8_t Read(uint32_t address, bool attribute) const;
  void Write(uint32_t address, bool attribute, uint8_t value);

  uint32_t Size() const { return static_cast<uint32_t>(common_.size()); }
  const std::vector<uint8_t>& Contents() const { return common_; }
  // True once anything was written since the last ClearModified().
  bool Modified() const { return modified_; }
  void ClearModified() { modified_ = false; }

  // The card's write-protect switch, 0480h bit 5 (FORMAT.EXE+06BAh).
  bool write_protected = false;
  // The card battery as BVD1/BVD2 report it, 0480h bits 2-3 (HANDOFF 6.8);
  // BKB's battery report warns about a weak and a flat one
  // (IDS_KBD_PCCARD_LOW/FLAT, 6.4).
  enum class Battery { kGood, kLow, kFlat };
  Battery battery = Battery::kGood;

 private:
  std::vector<uint8_t> common_;
  bool modified_ = false;
};

#endif
