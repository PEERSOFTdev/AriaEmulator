#include "pc_card.h"

PcCard PcCard::Sram(uint32_t size, std::vector<uint8_t> contents) {
  PcCard card;
  contents.resize(size, 0);
  card.common_ = std::move(contents);
  return card;
}

uint8_t PcCard::Read(uint32_t address, bool attribute) const {
  if (attribute || common_.empty()) return 0xFF;
  return common_[address & (Size() - 1)];
}

void PcCard::Write(uint32_t address, bool attribute, uint8_t value) {
  if (attribute || common_.empty() || write_protected) return;
  uint8_t& cell = common_[address & (Size() - 1)];
  if (cell == value) return;
  cell = value;
  modified_ = true;
}
