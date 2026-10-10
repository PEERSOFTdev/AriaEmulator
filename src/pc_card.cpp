#include "pc_card.h"

namespace {

// The CIS of an SRAM card: CISTPL_DEVICE (01h) with one device of type
// SRAM, then CISTPL_END. Device info byte: type in bits 4-7 (6 = SRAM),
// speed code in bits 0-2 (3 = 150 ns, a typical SRAM card of the time).
// Size byte: units - 1 in bits 3-7, unit code in bits 0-2 (512 B * 4^code),
// the smallest unit that keeps the count at 32 or less; 64 MB is 32 units
// of 2 MB, code 6, the largest the code reaches.
std::vector<uint8_t> SramCis(uint32_t size) {
  uint8_t code = 0;
  while (code < 6 && (512u << (2 * code)) * 32u < size) ++code;
  const uint32_t units = size / (512u << (2 * code));
  return {0x01, 0x03, 0x63, static_cast<uint8_t>(((units - 1) << 3) | code), 0xFF, 0xFF};
}

}  // namespace

PcCard PcCard::Sram(uint32_t size, std::vector<uint8_t> contents) {
  PcCard card;
  contents.resize(size, 0);
  card.common_ = std::move(contents);
  card.cis_ = SramCis(size);
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
  return common_[address & (Size() - 1)];
}

void PcCard::Write(uint32_t address, bool attribute, uint8_t value) {
  if (attribute || common_.empty() || write_protected) return;
  uint8_t& cell = common_[address & (Size() - 1)];
  if (cell == value) return;
  cell = value;
  modified_ = true;
  ++writes_;
}
