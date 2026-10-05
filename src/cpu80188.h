#ifndef ARIA_CPU80188_H
#define ARIA_CPU80188_H

#include <cstdint>

#include "i86/i86.h"

using CpuBus = mame_i86::I86Bus;

// The Aria's 80C188 (HANDOFF 6.12): MAME's i86/i186 core with its PCB
// peripherals (interrupt controller, three timers, two DMA channels, chip
// selects) behind a small interface. Memory and I/O go through CpuBus.
// Time is counted in CPU cycles; the timers count every fourth one.
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

  // Power-on reset: CS:IP = FFFF:0000, PCB at I/O FF00h, peripherals off.
  void Reset();
  // Runs one whole instruction, prefixes and REP repetitions included.
  // Returns the cycles it took by MAME's 80186 timing table; from HLT, the
  // cycles until the next timer event.
  int Step();
  // Runs for at least `cycles` cycles; returns the cycles actually used.
  int Run(int cycles);
  // CPU cycles since construction.
  int64_t Cycles() const { return m_clock_base + (m_icount_base - m_icount); }

  Registers GetRegisters() const;
  void SetRegisters(const Registers& registers);

  // Board-side pins of the 80188. INT0-INT3 go to the PCB interrupt
  // controller (the CPU's own INTR is driven only by it), NMI straight to
  // the core. DRQ0/DRQ1 request synchronised DMA transfers, TMRIN0/TMRIN1
  // clock or retrigger timers 0 and 1.
  void SetInterruptPin(int pin, bool asserted);
  void SetNmiLine(bool asserted);
  void SetDmaRequest(int channel, bool asserted);
  void SetTimerInput(int timer, bool asserted);

  bool Halted() const { return m_halt; }
  uint16_t RelocationRegister() const { return m_reloc; }
};

#endif
