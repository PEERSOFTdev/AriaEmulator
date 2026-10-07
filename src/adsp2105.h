#ifndef ARIA_ADSP2105_H
#define ARIA_ADSP2105_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "adsp2100/adsp2100.h"

// What the Aria board wires to the DSP besides its own chip (HANDOFF 6.3).
class Adsp2105Board {
 public:
  virtual ~Adsp2105Board() = default;
  // SPORT1 transmit register writes: where the audio samples go, most
  // likely to a DAC or codec (HANDOFF 6.3, not yet known).
  virtual void SportTransmit(int port, uint16_t data) {}
  virtual uint16_t SportReceive(int port) { return 0; }
  // Data memory outside the chip (the Aria's board has none known).
  virtual uint16_t ReadExternalData(uint32_t address) { return 0xFFFF; }
  virtual void WriteExternalData(uint32_t address, uint16_t data) {}
  virtual void Log(const char* text) {}
};

// The Aria's ADSP-2105 (HANDOFF 6.3): MAME's ADSP-21xx core (src/adsp2100/)
// with what MAME leaves to each machine: the on-chip program memory (1K
// words at 0000h-03FFh) and data memory (512 words at 3800h-39FFh), the
// control registers at 3C00h-3FFFh, and boot loading from a byte stream as
// the ADSP-2100 Family User's Manual describes it (10.4).
class Adsp2105 : private mame_adsp::adsp2101_device, private mame_adsp::DspBus {
 public:
  static constexpr int kProgramWords = 0x400;
  static constexpr uint32_t kDataBase = 0x3800, kDataWords = 0x200;
  static constexpr uint32_t kControlBase = 0x3C00;

  explicit Adsp2105(Adsp2105Board& board);

  // Hardware reset: registers to their reset values, PC = 0. The on-chip
  // memories keep their contents, as RAM does across a reset.
  void Reset();

  // Boot page loading (manual 10.4.1, 10.4.4): the first byte is the page
  // length L, then (L + 1) * 8 words follow from the last word down to
  // word 0, each as upper, lower, middle byte. Returns the number of bytes
  // the page takes, or 0 if `count` is too short; the words go to program
  // memory from address 0.
  std::size_t BootPage(const uint8_t* bytes, std::size_t count);

  // Runs at least `cycles` instruction cycles (one instruction each).
  int Run(int cycles);
  void SetInterrupt(int line, bool asserted);

  uint32_t Pc() const { return m_pc; }
  bool Idle() const { return m_idle != 0; }
  uint32_t ProgramWord(uint32_t address) const { return pm_[address % kProgramWords]; }
  uint16_t DataWord(uint32_t address) const;
  uint16_t ControlRegister(uint32_t address) const { return control_[address & 0x3FF]; }
  // Accesses outside the on-chip memories, for diagnostics.
  int OutOfRangeAccesses() const { return out_of_range_; }

 private:
  // DspBus.
  uint32_t ReadProgram(uint32_t address) override;
  void WriteProgram(uint32_t address, uint32_t data) override;
  uint16_t ReadData(uint32_t address) override;
  void WriteData(uint32_t address, uint16_t data) override;
  void SportTransmit(int port, uint16_t data) override { board_.SportTransmit(port, data); }
  uint16_t SportReceive(int port) override { return board_.SportReceive(port); }
  void Log(const char* text) override { board_.Log(text); }

  Adsp2105Board& board_;
  uint32_t pm_[kProgramWords] = {};
  uint16_t dm_[kDataWords] = {};
  uint16_t control_[0x400] = {};
  int out_of_range_ = 0;
};

#endif
