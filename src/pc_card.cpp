#include "pc_card.h"

namespace {

// 28F008SA: 1 MB, sixteen 64 KB blocks, Intel's identifier codes.
constexpr uint32_t kChipSize = PcCard::kFlashPairSize / 2;
constexpr uint32_t kChipBlock = 64u * 1024u;
constexpr uint8_t kIntel = 0x89;
constexpr uint8_t k28F008SA = 0xA2;

// Status register bits of a 28F008SA.
constexpr uint8_t kReady = 0x80;        // SR.7, write state machine ready.
constexpr uint8_t kEraseError = 0x20;   // SR.5; DLFFSA.EXE+13DBh, +145Fh test it.
constexpr uint8_t kProgramError = 0x10; // SR.4.
constexpr uint8_t kVppLow = 0x08;       // SR.3.

// The CIS: CISTPL_DEVICE (01h) with one device, then CISTPL_END. Device
// info byte: type in bits 4-7 (5 = flash, 6 = SRAM), speed code in bits
// 0-2 (3 = 150 ns, typical of the cards of the time). Size byte: units - 1
// in bits 3-7, unit code in bits 0-2 (512 B * 4^code), the smallest unit
// that keeps the count at 32 or less; 64 MB is 32 units of 2 MB, code 6,
// the largest the code reaches.
std::vector<uint8_t> Cis(uint8_t type, uint32_t size) {
  uint8_t code = 0;
  while (code < 6 && (512u << (2 * code)) * 32u < size) ++code;
  const uint32_t units = size / (512u << (2 * code));
  return {0x01, 0x03, static_cast<uint8_t>(type << 4 | 0x03), static_cast<uint8_t>(((units - 1) << 3) | code), 0xFF,
          0xFF};
}

}  // namespace

PcCard PcCard::Sram(uint32_t size, std::vector<uint8_t> contents) {
  PcCard card;
  contents.resize(size, 0);
  card.common_ = std::move(contents);
  card.cis_ = Cis(6, size);
  return card;
}

PcCard PcCard::Flash(uint32_t size, std::vector<uint8_t> contents) {
  PcCard card;
  card.kind_ = Kind::kFlash;
  contents.resize(size, 0xFF);
  card.common_ = std::move(contents);
  card.cis_ = Cis(5, size);
  // DLFFSA takes a flash card by its CIS only with CISTPL_JEDEC_C (18h)
  // beside CISTPL_DEVICE, and looks its chips up by the codes in it
  // (DLFFSA.EXE+089Eh-+0981h); Intel's cards carried one.
  card.cis_.insert(card.cis_.end() - 1, {0x18, 0x02, kIntel, k28F008SA});
  card.chips_.resize(size / kChipSize);
  return card;
}

uint8_t PcCard::Read(uint32_t address, bool attribute) const {
  if (common_.empty()) return 0xFF;
  if (attribute) {
    // Attribute memory holds a byte at every even address only; odd ones
    // and everything past the tuples read FFh.
    if (address & 1) return 0xFF;
    const uint32_t index = address / 2;
    return index < cis_.size() ? cis_[index] : 0xFF;
  }
  if (kind_ == Kind::kSram) return common_[address & (Size() - 1)];
  // No chip past the last pair: the bus floats.
  if (address >= Size()) return 0xFF;
  const Chip& chip = chips_[address / kChipSize / 2 * 2 + (address & 1)];
  switch (chip.mode) {
    case Chip::Mode::kArray: return common_[address];
    // Chip address bit 0 (card offset 2 on the even chip, DLFFSA.EXE+0511h)
    // picks the device code over the manufacturer's.
    case Chip::Mode::kId: return (address >> 1) & 1 ? k28F008SA : kIntel;
    // After a setup command, and in status mode, the chip shows its status.
    default: return chip.status;
  }
}

void PcCard::Change(uint32_t index, uint8_t value) {
  uint8_t& cell = common_[index];
  if (cell == value) return;
  cell = value;
  modified_ = true;
  ++writes_;
}

void PcCard::Write(uint32_t address, bool attribute, uint8_t value, bool vpp) {
  if (attribute || common_.empty() || write_protected) return;
  if (kind_ == Kind::kFlash) {
    FlashWrite(address, value, vpp);
    return;
  }
  Change(address & (Size() - 1), value);
}

void PcCard::FlashWrite(uint32_t address, uint8_t value, bool vpp) {
  if (address >= Size()) return;
  Chip& chip = chips_[address / kChipSize / 2 * 2 + (address & 1)];
  switch (chip.mode) {
    case Chip::Mode::kProgram:
      // Programming only clears bits; setting one takes an erase.
      if (vpp) {
        Change(address, common_[address] & value);
      } else {
        chip.status |= kVppLow | kProgramError;
      }
      chip.mode = Chip::Mode::kStatus;
      return;
    case Chip::Mode::kErase:
      if (value != 0xD0) {
        // Anything but the confirm is a command sequence error.
        chip.status |= kEraseError | kProgramError;
      } else if (!vpp) {
        chip.status |= kVppLow | kEraseError;
      } else {
        // The block of this chip that holds `address`: its bytes are every
        // other byte of the card, from the pair's start.
        const uint32_t pair = address / kFlashPairSize * kFlashPairSize;
        const uint32_t first = ((address - pair) / 2) / kChipBlock * kChipBlock;
        for (uint32_t i = 0; i < kChipBlock; ++i) Change(pair + 2 * (first + i) + (address & 1), 0xFF);
      }
      chip.mode = Chip::Mode::kStatus;
      return;
    default:
      break;
  }
  switch (value) {
    case 0x90: chip.mode = Chip::Mode::kId; return;
    case 0x70: chip.mode = Chip::Mode::kStatus; return;
    case 0x50: chip.status = kReady; return;
    case 0x40:
    case 0x10: chip.mode = Chip::Mode::kProgram; return;
    case 0x20: chip.mode = Chip::Mode::kErase; return;
    // Suspend and resume: nothing runs long enough to suspend.
    case 0xB0:
    case 0xD0: return;
    // FFh is read array; DLFFSA also writes 00h and AMD's F0h and unlock
    // bytes (DLFFSA.EXE+0518h, +1431h), which a 28F008SA does not know and
    // which leave it reading the array as well.
    default: chip.mode = Chip::Mode::kArray; return;
  }
}
