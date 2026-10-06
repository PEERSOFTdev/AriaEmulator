#include "cpu80188.h"

namespace mame_i86 {
// CompressFlags/ExpandFlags live with the rest of MAME's inline helpers.
#include "i86/i86inline.h"
}  // namespace mame_i86

Cpu80188::Cpu80188(CpuBus& bus) {
  bus_ = &bus;
  // MAME leaves m_icount uninitialised (its scheduler sets it before each
  // slice). Our cycle clock reads it between runs too, so with garbage
  // there a timer programmed before the first run (the BIOS's tick) was
  // scheduled against a wrong clock and never fired (HANDOFF 6.12).
  m_icount = 0;
  m_icount_base = 0;
  Reset();
}

void Cpu80188::Reset() {
  i80186_cpu_device::device_reset();
}

int Cpu80188::Step() {
  // Generous budget: a REP string instruction must run to completion here
  // rather than stop at the end of the budget, which MAME does by moving
  // IP back to the instruction for the next call.
  constexpr int kBudget = 1 << 24;
  BeginRun(kBudget);
  m_single_step = true;
  execute_run();
  m_single_step = false;
  const int used = kBudget - m_icount;
  EndRun();
  return used;
}

int Cpu80188::Run(int cycles) {
  BeginRun(cycles);
  execute_run();
  const int used = cycles - m_icount;
  EndRun();
  return used;
}

Cpu80188::Registers Cpu80188::GetRegisters() const {
  Registers r;
  r.ax = m_regs.w[AX];
  r.bx = m_regs.w[BX];
  r.cx = m_regs.w[CX];
  r.dx = m_regs.w[DX];
  r.cs = m_sregs[CS];
  r.ss = m_sregs[SS];
  r.ds = m_sregs[DS];
  r.es = m_sregs[ES];
  r.sp = m_regs.w[SP];
  r.bp = m_regs.w[BP];
  r.si = m_regs.w[SI];
  r.di = m_regs.w[DI];
  r.ip = m_ip;
  r.flags = CompressFlags();
  return r;
}

void Cpu80188::SetRegisters(const Registers& r) {
  m_regs.w[AX] = r.ax;
  m_regs.w[BX] = r.bx;
  m_regs.w[CX] = r.cx;
  m_regs.w[DX] = r.dx;
  m_sregs[CS] = r.cs;
  m_sregs[SS] = r.ss;
  m_sregs[DS] = r.ds;
  m_sregs[ES] = r.es;
  m_regs.w[SP] = r.sp;
  m_regs.w[BP] = r.bp;
  m_regs.w[SI] = r.si;
  m_regs.w[DI] = r.di;
  m_ip = m_prev_ip = r.ip;
  ExpandFlags(r.flags);
}

void Cpu80188::SetInterruptPin(int pin, bool asserted) {
  UpdateClock();
  external_int(static_cast<uint16_t>(pin), asserted ? mame_i86::ASSERT_LINE : mame_i86::CLEAR_LINE);
}

void Cpu80188::SetDmaRequest(int channel, bool asserted) {
  m_dma[channel].drq_state = asserted;
}

void Cpu80188::SetTimerInput(int timer, bool asserted) {
  UpdateClock();
  external_tmrin(timer, asserted ? mame_i86::ASSERT_LINE : mame_i86::CLEAR_LINE);
}

void Cpu80188::SetNmiLine(bool asserted) {
  execute_set_input(mame_i86::INPUT_LINE_NMI,
                    asserted ? mame_i86::ASSERT_LINE : mame_i86::CLEAR_LINE);
}
