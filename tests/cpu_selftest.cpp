// The 80C188 facade (src/cpu80188.*) without the SingleStepTests data:
// small hand-assembled programs for what the Aria relies on beyond single
// instructions. Prints PASS on success.

#include <iostream>
#include <utility>
#include <vector>

#include "cpu80188.h"

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL " << what << "\n";
    ++failures;
  }
}

class Bus : public CpuBus {
 public:
  Bus() : memory(1 << 20, 0) {}

  void Load(uint32_t address, std::vector<uint8_t> bytes) {
    for (uint8_t byte : bytes) memory[address++] = byte;
  }
  void Vector(int number, uint16_t segment, uint16_t offset) {
    Load(number * 4, {static_cast<uint8_t>(offset), static_cast<uint8_t>(offset >> 8),
                      static_cast<uint8_t>(segment), static_cast<uint8_t>(segment >> 8)});
  }

  uint8_t ReadMemory(uint32_t address) override { return memory[address]; }
  void WriteMemory(uint32_t address, uint8_t value) override { memory[address] = value; }
  uint8_t ReadIo(uint16_t) override { return 0xFF; }
  void WriteIo(uint16_t port, uint8_t value) override { writes.emplace_back(port, value); }
  uint8_t AcknowledgeInterrupt() override {
    ++acknowledged;
    if (cpu) cpu->SetInterruptLine(false);  // Edge: one request.
    return 0x40;
  }

  std::vector<uint8_t> memory;
  std::vector<std::pair<uint16_t, uint8_t>> writes;
  int acknowledged = 0;
  Cpu80188* cpu = nullptr;
};

bool RunUntilHalt(Cpu80188& cpu) {
  for (int step = 0; step < 1000; ++step) {
    if (cpu.Halted()) return true;
    cpu.Step();
  }
  return cpu.Halted();
}

Cpu80188::Registers At(uint16_t cs, uint16_t ip) {
  Cpu80188::Registers r;
  r.cs = cs;
  r.ip = ip;
  r.ss = 0x0200;
  r.sp = 0x0100;
  r.flags = 0xF002;
  return r;
}

void TestReset() {
  Bus bus;
  Cpu80188 cpu(bus);
  const auto r = cpu.GetRegisters();
  Check(r.cs == 0xFFFF && r.ip == 0x0000, "reset vector FFFF:0000");
  Check(cpu.RelocationRegister() == 0x20FF, "PCB at I/O FF00h after reset");
}

void TestProgram() {
  Bus bus;
  Cpu80188 cpu(bus);
  bus.Load(0x01000, {
      0xB8, 0x34, 0x12,  // mov ax,1234h
      0x50,              // push ax
      0x5B,              // pop bx
      0xB9, 0x03, 0x00,  // mov cx,3
      0x43,              // inc bx          <- loop
      0xE2, 0xFD,        // loop -3
      0xE6, 0x80,        // out 80h,al      (board port, PCB still at FF00h)
      0xBA, 0xFE, 0xFF,  // mov dx,FFFEh    (PCB relocation register)
      0x31, 0xC0,        // xor ax,ax
      0xEF,              // out dx,ax       -> PCB to I/O 0000h
      0xE6, 0x80,        // out 80h,al      (now inside the PCB, not the board)
      0xBA, 0x00, 0x06,  // mov dx,0600h
      0xEE,              // out dx,al       (board port)
      0xCD, 0x21,        // int 21h
      0xF4,              // hlt
  });
  bus.Vector(0x21, 0x0000, 0x0500);
  bus.Load(0x00500, {0xB0, 0x77, 0xCF});  // mov al,77h; iret
  cpu.SetRegisters(At(0x0100, 0x0000));
  Check(RunUntilHalt(cpu), "program halts");
  const auto r = cpu.GetRegisters();
  Check(r.ax == 0x0077 && r.bx == 0x1237 && r.cx == 0, "loop, stack and INT 21h");
  Check(r.sp == 0x0100 && r.ip == 0x001C, "stack balanced, IP after HLT");
  Check(cpu.RelocationRegister() == 0x0000, "relocation register written through OUT DX,AX");
  const std::vector<std::pair<uint16_t, uint8_t>> expected = {{0x0080, 0x34}, {0x0600, 0x00}};
  Check(bus.writes == expected, "board sees port 80h only before the PCB moved there");
}

void TestHardwareInterrupt() {
  Bus bus;
  Cpu80188 cpu(bus);
  bus.cpu = &cpu;
  bus.Load(0x01000, {0xFB, 0x90, 0xF4});  // sti; nop; hlt
  bus.Vector(0x40, 0x0000, 0x0600);
  bus.Load(0x00600, {0xB3, 0x55, 0xCF});  // mov bl,55h; iret
  cpu.SetRegisters(At(0x0100, 0x0000));
  cpu.SetInterruptLine(true);
  Check(RunUntilHalt(cpu), "interrupt program halts");
  Check(bus.acknowledged == 1 && (cpu.GetRegisters().bx & 0xFF) == 0x55, "INTR taken once after STI");
}

void TestEscapeTrap() {
  // With bit 15 of the relocation register set, ESC opcodes trap to INT 7
  // with IP on the ESC (HANDOFF 6.12). The handler is VOICE.SYS's own
  // (VOICE.SYS+01C5h: push bp; mov bp,sp; add word [bp+2],2; pop bp; iret)
  // behind an inc si that counts the traps.
  Bus bus;
  Cpu80188 cpu(bus);
  bus.Load(0x01000, {
      0xBA, 0xFE, 0xFF,  // mov dx,FFFEh
      0xB8, 0x00, 0x80,  // mov ax,8000h  (ESC trap on, PCB to 0000h)
      0xEF,              // out dx,ax
      0xD8, 0xC0,        // fadd st,st(0) (ESC with register operand)
      0xF4,              // hlt
  });
  bus.Vector(7, 0x0000, 0x0700);
  bus.Load(0x00700, {0x46, 0x55, 0x8B, 0xEC, 0x83, 0x46, 0x02, 0x02, 0x5D, 0xCF});
  cpu.SetRegisters(At(0x0100, 0x0000));
  Check(RunUntilHalt(cpu), "ESC program halts");
  const auto r = cpu.GetRegisters();
  Check(r.si == 1 && r.ip == 0x000A, "ESC traps once to INT 7 and VOICE.SYS's handler skips it");
}

}  // namespace

int main() {
  TestReset();
  TestProgram();
  TestHardwareInterrupt();
  TestEscapeTrap();
  std::cout << (failures == 0 ? "PASS" : "FAIL") << " cpu_selftest\n";
  return failures == 0 ? 0 : 1;
}
