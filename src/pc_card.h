#ifndef ARIA_PC_CARD_H
#define ARIA_PC_CARD_H

#include <cstdint>
#include <vector>

// A memory card in the Aria's PCMCIA slot, drive C: (HANDOFF 6.8). The
// board shows it through the D000h window in mode 3; the original drivers
// from drive B: (DLFFSA.EXE, TRICKSS2.EXE, FORMAT.EXE) do everything else.
//
// Two kinds: SRAM (bead ar-3ap), plain read/write memory kept by the
// card's own battery, and flash (ar-ibv), pairs of Intel 28F008SA chips
// that take Intel's commands.
class PcCard {
 public:
  enum class Kind { kSram, kFlash };

  // An SRAM card of `size` bytes, which must be a power of two of at least
  // 64 KB: the card mirrors its memory above its size, as an address
  // decoder with fewer lines does, and FORMAT.EXE finds the size by that
  // wrap in 64 KB steps (FORMAT.EXE+25D0h). Shorter `contents` is padded.
  static PcCard Sram(uint32_t size, std::vector<uint8_t> contents = {});

  // A flash card of `size` bytes, a multiple of kFlashPairSize: pairs of
  // Intel 28F008SA, the one chip DLFFSA, TRICKSS2 and FORMAT all know
  // (HANDOFF 6.8), even bytes on one chip of a pair and odd on the other.
  // It does not mirror: DLFFSA counts the pairs by reading the chip ID at
  // every multiple of the pair size until it no longer answers
  // (DLFFSA.EXE+0680h), so past the last pair the card reads FFh. Shorter
  // `contents` is padded with FFh, erased flash.
  static constexpr uint32_t kFlashPairSize = 2u * 1024u * 1024u;
  static PcCard Flash(uint32_t size, std::vector<uint8_t> contents = {});

  // REG# low (0480h bit 7 = 0) selects attribute memory, where a card keeps
  // its CIS (FORMAT.EXE+1DA9h), one byte per even address; writes are
  // ignored. The drivers take a card for SRAM by writing to common memory
  // and only then look at the CIS (FORMAT.EXE+2954h), so a card locked
  // since power-on is known as SRAM only by its CIS: without one, every
  // access said "celkové selhání c" (HANDOFF 6.8). Both kinds therefore
  // carry a CISTPL_DEVICE naming their type and size, as real ones did.
  uint8_t Read(uint32_t address, bool attribute) const;
  // `vpp` is the slot's programming voltage, 0702h bit 0 (DLFFSA.EXE+0D58h
  // switches it on around flash programming): without it a flash chip
  // fails program and erase with SR.3, as a real 28F008SA does.
  void Write(uint32_t address, bool attribute, uint8_t value, bool vpp = true);

  Kind kind() const { return kind_; }
  uint32_t Size() const { return static_cast<uint32_t>(common_.size()); }
  const std::vector<uint8_t>& Contents() const { return common_; }
  // True once anything was written since the last ClearModified().
  bool Modified() const { return modified_; }
  void ClearModified() { modified_ = false; }
  // Bytes changed since the card was made: the host saves a folder card
  // once this has stood still for a while, as the Eureka emulator saves a
  // diskette after a second of quiet.
  uint64_t Writes() const { return writes_; }

  // The card's write-protect switch, 0480h bit 5 (FORMAT.EXE+06BAh).
  bool write_protected = false;
  // The card battery as BVD1/BVD2 report it, 0480h bits 2-3 (HANDOFF 6.8);
  // BKB's battery report warns about a weak and a flat one
  // (IDS_KBD_PCCARD_LOW/FLAT, 6.4). A flash card has none and reports good.
  enum class Battery { kGood, kLow, kFlat };
  Battery battery = Battery::kGood;

 private:
  // One 28F008SA: which of its read modes is on, or which command waits
  // for its second write, and its status register.
  struct Chip {
    enum class Mode { kArray, kId, kStatus, kProgram, kErase };
    Mode mode = Mode::kArray;
    uint8_t status = 0x80;  // SR.7 ready; operations finish at once.
  };

  void Change(uint32_t index, uint8_t value);
  void FlashWrite(uint32_t address, uint8_t value, bool vpp);

  Kind kind_ = Kind::kSram;
  std::vector<uint8_t> common_;
  std::vector<uint8_t> cis_;  // Attribute memory, one byte per even address.
  std::vector<Chip> chips_;   // Flash: two per pair, even byte first.
  bool modified_ = false;
  uint64_t writes_ = 0;
};

#endif
