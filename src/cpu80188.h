#ifndef ARIA_CPU80188_H
#define ARIA_CPU80188_H

#include <cstdint>

#include "i86/i86.h"

using CpuBus = mame_i86::I86Bus;

// The Aria's 80C188 (HANDOFF 6.12): MAME's i86/i186 core behind a small
// interface. Memory and I/O go through CpuBus; the PCB peripherals are not
// emulated yet, except the relocation register.
class Cpu80188 : private mame_i86::i80186_cpu_device {
 public:
  // The order of the SingleStepTests register mask (HANDOFF 6.12).
  struct Registers {
    uint16_t ax = 0, bx = 0, cx = 0, dx = 0;
    uint16_t cs = 0, ss = 0, ds = 0, es = 0;
    uint16_t sp = 0, bp = 0, si = 0, di = 0;
    uint16_t ip = 0, flags = 0;
  };

  explicit Cpu80188(CpuBus& bus);

  // Power-on reset: CS:IP = FFFF:0000, PCB at I/O FF00h.
  void Reset();
  // Runs one whole instruction, prefixes and REP repetitions included.
  // Returns the cycles it took by MAME's 80186 timing table.
  int Step();
  // Runs for at least `cycles` cycles; returns the cycles actually used.
  int Run(int cycles);

  Registers GetRegisters() const;
  void SetRegisters(const Registers& registers);

  // INTR line level and NMI edge, as the PCB or the board drives them.
  void SetInterruptLine(bool asserted);
  void SetNmiLine(bool asserted);

  bool Halted() const { return m_halt; }
  uint16_t RelocationRegister() const { return m_reloc; }
};

#endif
