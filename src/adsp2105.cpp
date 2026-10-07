#include "adsp2105.h"

Adsp2105::Adsp2105(Adsp2105Board& board)
    : adsp2101_device(CHIP_TYPE_ADSP2105), board_(board) {
  m_bus = this;
  Reset();
}

void Adsp2105::Reset() {
  device_reset();
}

std::size_t Adsp2105::BootPage(const uint8_t* bytes, std::size_t count) {
  if (count < 1) return 0;
  const std::size_t words = (static_cast<std::size_t>(bytes[0]) + 1) * 8;
  const std::size_t needed = 1 + 3 * words;
  if (count < needed || words > kProgramWords) return 0;
  for (std::size_t k = 0; k < words; ++k) {
    const uint8_t upper = bytes[1 + 3 * k];
    const uint8_t lower = bytes[2 + 3 * k];
    const uint8_t middle = bytes[3 + 3 * k];
    pm_[words - 1 - k] = static_cast<uint32_t>(upper) << 16 | static_cast<uint32_t>(middle) << 8 | lower;
  }
  return needed;
}

int Adsp2105::Run(int cycles) {
  m_icount = cycles;
  execute_run();
  return cycles - m_icount;
}

void Adsp2105::SetInterrupt(int line, bool asserted) {
  execute_set_input(line, asserted ? mame_adsp::ASSERT_LINE : mame_adsp::CLEAR_LINE);
}

uint16_t Adsp2105::DataWord(uint32_t address) const {
  address &= 0x3FFF;
  if (address >= kDataBase && address < kDataBase + kDataWords) return dm_[address - kDataBase];
  if (address >= kControlBase) return control_[address - kControlBase];
  return 0xFFFF;
}

uint32_t Adsp2105::ReadProgram(uint32_t address) {
  // MMAP = 0 (booted at reset): on-chip program RAM at 0000h-03FFh, 0400h-
  // 07FFh reserved, external memory above (MAME adsp2100.cpp header).
  if (address < kProgramWords) return pm_[address];
  ++out_of_range_;
  return 0;
}

void Adsp2105::WriteProgram(uint32_t address, uint32_t data) {
  if (address < kProgramWords) {
    pm_[address] = data & 0xFFFFFF;
    return;
  }
  ++out_of_range_;
}

uint16_t Adsp2105::ReadData(uint32_t address) {
  if (address >= kDataBase && address < kDataBase + kDataWords) return dm_[address - kDataBase];
  if (address >= kControlBase) return control_[address - kControlBase];
  if (address < 0x3800) return board_.ReadExternalData(address);
  ++out_of_range_;
  return 0;
}

void Adsp2105::WriteData(uint32_t address, uint16_t data) {
  if (address >= kDataBase && address < kDataBase + kDataWords) {
    dm_[address - kDataBase] = data;
    return;
  }
  if (address >= kControlBase) {
    // Control registers (SPORT, timer, wait states, system control) are
    // only stored for now; their behaviour comes with the board model.
    control_[address - kControlBase] = data;
    return;
  }
  if (address < 0x3800) {
    board_.WriteExternalData(address, data);
    return;
  }
  ++out_of_range_;
}
