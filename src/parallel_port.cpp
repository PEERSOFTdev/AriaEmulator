#include "parallel_port.h"

namespace {

// Aria control bit i carries PC control bit kControlBits[i] (BACKPACK.SYS
// +19C9h); status bit i carries PC status bit kStatusBits[i] (+18C9h).
constexpr int kControlBits[4] = {0, 3, 1, 2};
constexpr int kStatusBits[5] = {5, 7, 6, 4, 3};

uint8_t ToPcControl(uint8_t aria) {
  uint8_t pc = 0;
  for (int i = 0; i < 4; ++i) {
    if (aria & (1 << i)) pc |= static_cast<uint8_t>(1 << kControlBits[i]);
  }
  return pc;
}

uint8_t ToAriaStatus(uint8_t pc) {
  // The PC's bit 7 is BUSY inverted; the Aria's line is not.
  pc ^= 0x80;
  uint8_t aria = 0;
  for (int i = 0; i < 5; ++i) {
    if (pc & (1 << kStatusBits[i])) aria |= static_cast<uint8_t>(1 << i);
  }
  return aria;
}

}  // namespace

uint8_t ParallelPort::Read(int reg) {
  if (reg == 2) return 0xFF;  // Nobody reads control back (BACKPACK.SYS keeps a copy).
  // Nothing connected reads 7Fh, as an empty PC port does.
  const uint8_t pc_status = device_ ? device_->Status() : 0x7F;
  if (trace) trace(1, pc_status, false);
  return ToAriaStatus(pc_status);
}

void ParallelPort::Write(int reg, uint8_t value) {
  if (reg == 1) {
    if (trace) trace(0, value, true);
    if (device_) device_->Data(value);
  } else if (reg == 2) {
    pc_control_ = ToPcControl(value & 0x0F);
    if (trace) trace(2, pc_control_, true);
    if (device_) device_->Control(pc_control_);
  }
}
