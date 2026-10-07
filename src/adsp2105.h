#ifndef ARIA_ADSP2105_H
#define ARIA_ADSP2105_H

#include <cstddef>
#include <cstdint>

#include "adsp2100/adsp2100.h"

// What the Aria board wires to the DSP besides its own chip (HANDOFF 6.3).
class Adsp2105Board {
 public:
  virtual ~Adsp2105Board() = default;
  // A word shifted out of SPORT1: one sample for the DAC (HANDOFF 6.3),
  // with the SCLKDIV it was sent at: the Aria's frame sync is derived
  // from the serial clock, so the divider sets the sample's duration.
  virtual void SportOutput(uint16_t sample, uint16_t clock_divider) {}
  // Data memory outside the chip: the host mailbox at 3000h on the Aria.
  virtual uint16_t ReadExternalData(uint32_t address) { return 0xFFFF; }
  virtual void WriteExternalData(uint32_t address, uint16_t data) {}
  virtual void Log(const char* text) {}
};

// The Aria's ADSP-2105 (HANDOFF 6.3): MAME's ADSP-21xx core (src/adsp2100/)
// with what MAME leaves to each machine, from the ADSP-2100 Family User's
// Manual (3rd ed., 1995): the on-chip program memory (1K words at 0000h-
// 03FFh) and data memory (512 words at 3800h-39FFh), the control registers
// at 3C00h-3FFFh, boot loading (10.4), IDLE waiting for an interrupt, and
// SPORT1 transmission with autobuffering (5.11).
class Adsp2105 : private mame_adsp::adsp2101_device, private mame_adsp::DspBus {
 public:
  static constexpr int kProgramWords = 0x400;
  static constexpr uint32_t kDataBase = 0x3800, kDataWords = 0x200;
  static constexpr uint32_t kControlBase = 0x3C00;
  // Interrupt inputs as MAME numbers them (adsp2100.h).
  static constexpr int kIrq2 = mame_adsp::ADSP2105_IRQ2;

  explicit Adsp2105(Adsp2105Board& board);

  // Hardware reset: registers and control registers to their reset values,
  // PC = 0. The on-chip memories keep their contents, as RAM does.
  void Reset();

  // Boot page loading (manual 10.4.1, 10.4.4): the first byte is the page
  // length L, then (L + 1) * 8 words follow from the last word down to
  // word 0, each as upper, lower, middle byte, into program memory from 0.
  // BootPage takes a whole page and returns the bytes used (0 if too
  // short); BeginBoot/BootByte take it byte by byte, as the Aria's host
  // latch delivers it, and BootByte returns true after the last byte.
  std::size_t BootPage(const uint8_t* bytes, std::size_t count);
  void BeginBoot();
  bool BootByte(uint8_t byte);
  bool Booting() const { return boot_remaining_ > 0 || boot_expect_length_; }

  // Runs `cycles` instruction cycles (one instruction each), with the
  // SPORT and IDLE timing. Returns the cycles used.
  int Run(int cycles);
  void SetInterrupt(int line, bool asserted);

  uint32_t Pc() const { return m_pc; }
  bool Idle() const { return m_idle != 0; }
  uint32_t ProgramWord(uint32_t address) const { return pm_[address % kProgramWords]; }
  uint16_t DataWord(uint32_t address) const;
  uint16_t ControlRegister(uint32_t address) const { return control_[address & 0x3FF]; }
  // Accesses outside the on-chip memories and the board, for diagnostics.
  int OutOfRangeAccesses() const { return out_of_range_; }
  // Instruction cycles one SPORT1 word takes with the current SCLKDIV.
  int SportWordCycles() const;
  // Cycles spent executing rather than waiting in IDLE, since construction.
  int64_t BusyCycles() const { return busy_cycles_; }

 private:
  // DspBus.
  uint32_t ReadProgram(uint32_t address) override;
  void WriteProgram(uint32_t address, uint32_t data) override;
  uint16_t ReadData(uint32_t address) override;
  void WriteData(uint32_t address, uint16_t data) override;
  void SportTransmit(int port, uint16_t data) override;
  uint16_t SportReceive(int port) override { return 0; }
  void Log(const char* text) override { board_.Log(text); }

  // The SPORT1 transmit interrupt is internal and latched, unlike the IRQ1
  // pin MAME files it under; it is cleared when taken.
  bool generate_irq(int which, int indx) override;

  bool Sport1Enabled() const;
  void SportWordDone();

  Adsp2105Board& board_;
  uint32_t pm_[kProgramWords] = {};
  uint16_t dm_[kDataWords] = {};
  uint16_t control_[0x400] = {};
  int out_of_range_ = 0;
  int64_t busy_cycles_ = 0;

  // Boot loading state.
  bool boot_expect_length_ = false;
  std::size_t boot_remaining_ = 0;  // Bytes left in the page.
  std::size_t boot_words_ = 0;
  uint8_t boot_bytes_[3] = {};
  int boot_byte_index_ = 0;

  // SPORT1 transmitter: TX1 holds the next word, the shift register the
  // word being sent; `shift_left_` counts its remaining cycles.
  bool tx_full_ = false;
  uint16_t tx_ = 0;
  bool shifting_ = false;
  uint16_t shift_ = 0;
  int shift_left_ = 0;
};

#endif
