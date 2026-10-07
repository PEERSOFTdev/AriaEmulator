#include "adsp2105.h"

#include <cstdio>

namespace {

// Control registers (ADSP-2100 Family User's Manual, 3rd ed.).
constexpr uint32_t kSysControl = 0x3FF;    // 3FFFh, figure 5.2 and 10.17.
constexpr uint32_t kSport1Control = 0x3F2; // 3FF2h, figure 5.3.
constexpr uint32_t kSport1ClockDiv = 0x3F1;  // 3FF1h, SCLKDIV.
constexpr uint32_t kSport1Autobuffer = 0x3EF;  // 3FEFh, figure 5.23.

// System control: SPORT1 enable (bit 11) and SPORT1 configured as a serial
// port rather than FI/FO/IRQ0/IRQ1 (bit 10), 5.4.
constexpr uint16_t kSport1Enable = 0x0800, kSport1Serial = 0x0400;
// Reset value: SPORTs disabled, SPORT1 a serial port, BWAIT = 3 (10.4.3),
// PWAIT = 7.
constexpr uint16_t kSysControlReset = kSport1Serial | 3 << 3 | 7;

}  // namespace

Adsp2105::Adsp2105(Adsp2105Board& board)
    : adsp2101_device(CHIP_TYPE_ADSP2105), board_(board) {
  m_bus = this;
  Reset();
}

void Adsp2105::Reset() {
  device_reset();
  // ICNTL is "undefined after a processor reset" (manual 3.4.2.1) and
  // MAME's reset keeps it. PLAY.DSP never sets it, yet both its interrupt
  // handlers use the secondary register bank (ENA SEC_REG at 00C6h and
  // 011Bh), which is only safe without nesting; with LPC.DSP's ICNTL = 14h
  // left over, IRQ2 nested into the SPORT1 handler and corrupted AR
  // between 0120h and 0123h (HANDOFF 6.3). Assumption: 0 after reset.
  m_icntl = 0;
  for (uint16_t& reg : control_) reg = 0;
  control_[kSysControl] = kSysControlReset;
  tx_full_ = shifting_ = false;
  shift_left_ = 0;
  boot_expect_length_ = false;
  boot_remaining_ = 0;
}

std::size_t Adsp2105::BootPage(const uint8_t* bytes, std::size_t count) {
  if (count < 1) return 0;
  const std::size_t needed = 1 + 3 * ((static_cast<std::size_t>(bytes[0]) + 1) * 8);
  if (count < needed || needed > 1 + 3 * kProgramWords) return 0;
  BeginBoot();
  for (std::size_t i = 0; i < needed; ++i) BootByte(bytes[i]);
  return needed;
}

void Adsp2105::BeginBoot() {
  boot_expect_length_ = true;
  boot_remaining_ = 0;
  boot_byte_index_ = 0;
}

bool Adsp2105::BootByte(uint8_t byte) {
  if (boot_expect_length_) {
    boot_expect_length_ = false;
    boot_words_ = (static_cast<std::size_t>(byte) + 1) * 8;
    if (boot_words_ > kProgramWords) boot_words_ = kProgramWords;
    boot_remaining_ = 3 * boot_words_;
    boot_byte_index_ = 0;
    return false;
  }
  if (boot_remaining_ == 0) return false;
  boot_bytes_[boot_byte_index_++] = byte;
  --boot_remaining_;
  if (boot_byte_index_ == 3) {
    // Words arrive from the last one down: upper, lower, middle byte.
    const std::size_t word = boot_remaining_ / 3;
    pm_[word] = static_cast<uint32_t>(boot_bytes_[0]) << 16 | static_cast<uint32_t>(boot_bytes_[2]) << 8 |
                boot_bytes_[1];
    boot_byte_index_ = 0;
  }
  return boot_remaining_ == 0;
}

bool Adsp2105::Sport1Enabled() const {
  const uint16_t sys = control_[kSysControl];
  return (sys & kSport1Serial) && (sys & kSport1Enable);
}

int Adsp2105::SportWordCycles() const {
  // SCLK = CLKOUT / (2 * (SCLKDIV + 1)) with the internal clock (5.5). The
  // Aria's transmit frame sync is external (4C8Fh: ITFS = 0), so the board
  // decides how many SCLK periods a frame lasts. Assumption: 32, a 16-bit
  // sample and 16 idle bits; with the 10.24 MHz clock that makes Czech
  // speech (SCLKDIV 4) 32 kHz and the harp (19) 8 kHz, which is what
  // listening chose (HANDOFF 6.3).
  constexpr int kBitsPerFrame = 32;
  return kBitsPerFrame * 2 * (control_[kSport1ClockDiv] + 1);
}

void Adsp2105::SportTransmit(int port, uint16_t data) {
  if (port != 1) return;  // The 2105 has SPORT1 only.
  tx_ = data;
  tx_full_ = true;
}

void Adsp2105::SportWordDone() {
  board_.SportOutput(shift_, control_[kSport1ClockDiv]);
  shifting_ = false;
}

bool Adsp2105::generate_irq(int which, int indx) {
  const bool taken = adsp2101_device::generate_irq(which, indx);
  if (taken && which == mame_adsp::ADSP2105_SPORT1_TX && (control_[kSysControl] & kSport1Serial)) {
    m_irq_state[which] = mame_adsp::CLEAR_LINE;
  }
  return taken;
}

int Adsp2105::Run(int cycles) {
  int used = 0;
  while (used < cycles) {
    // Move a word from TX1 into the shift register and, with transmit
    // autobuffering (TBUF, figure 5.23), refill TX1 from data memory
    // through the I and M registers named in the autobuffer register; the
    // transmit interrupt follows when the I register wraps (5.11). Without
    // autobuffering the interrupt follows each word taken from TX1.
    if (Sport1Enabled() && !shifting_ && tx_full_) {
      shift_ = tx_;
      tx_full_ = false;
      shifting_ = true;
      shift_left_ = SportWordCycles();
      const uint16_t autobuffer = control_[kSport1Autobuffer];
      if (autobuffer & 0x0002) {
        const int ireg = autobuffer >> 9 & 7;
        const int mreg = (autobuffer >> 7 & 3) | (ireg & 4);  // M from the same DAG.
        bool wrapped = false;
        tx_ = autobuffer_read(ireg, mreg, wrapped);
        tx_full_ = true;
        if (wrapped) execute_set_input(mame_adsp::ADSP2105_SPORT1_TX, mame_adsp::ASSERT_LINE);
      } else {
        execute_set_input(mame_adsp::ADSP2105_SPORT1_TX, mame_adsp::ASSERT_LINE);
      }
    }

    int slice = cycles - used;
    if (shifting_ && shift_left_ < slice) slice = shift_left_ > 0 ? shift_left_ : 1;

    int ran;
    if (m_idle) {
      // IDLE waits for an interrupt (the core only ends the slice there).
      check_irqs();
      ran = m_idle ? slice : 0;
    } else {
      ran = 0;
    }
    if (!m_idle && ran == 0) {
      m_icount = slice;
      m_idle_icount = 0;
      execute_run();
      // IDLE ends the slice by zeroing the count; what was left is kept.
      ran = slice - (m_idle ? m_idle_icount : m_icount);
      if (ran <= 0) ran = 1;
      busy_cycles_ += ran;
    }
    used += ran;
    cycles_ += ran;
    if (shifting_) {
      shift_left_ -= ran;
      if (shift_left_ <= 0) SportWordDone();
    }
  }
  return used;
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
    control_[address - kControlBase] = data;
    if (address - kControlBase == kSysControl) {
      char text[80];
      std::snprintf(text, sizeof text, "ADSP %04X: system control 3FFFh <- %04X\n", m_ppc, data);
      board_.Log(text);
      if (data & 0x0200) board_.Log("ADSP: BFORCE (software reboot) is not modelled\n");
    }
    return;
  }
  if (address < 0x3800) {
    board_.WriteExternalData(address, data);
    return;
  }
  ++out_of_range_;
}
