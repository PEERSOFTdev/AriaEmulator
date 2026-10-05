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

  std::vector<uint8_t> memory;
  std::vector<std::pair<uint16_t, uint8_t>> writes;
};

// mov dx,FFFEh; xor ax,ax; out dx,ax: the PCB to I/O 0000h, as the Aria
// has it (HANDOFF 6.1). Six bytes.
const std::vector<uint8_t> kRelocatePcb = {0xBA, 0xFE, 0xFF, 0x31, 0xC0, 0xEF};

// mov dx,PORT; mov ax,VALUE; out dx,ax. Seven bytes.
std::vector<uint8_t> OutWord(uint16_t port, uint16_t value) {
  return {0xBA, static_cast<uint8_t>(port), static_cast<uint8_t>(port >> 8), 0xB8,
          static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8), 0xEF};
}

std::vector<uint8_t> Join(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> code;
  for (const auto& part : parts) code.insert(code.end(), part.begin(), part.end());
  return code;
}

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

void TestExternalInterrupt() {
  // INT0 through the PCB interrupt controller: type 0Ch, EOI to port 22h.
  // The second request only gets through if the EOI cleared in-service.
  Bus bus;
  Cpu80188 cpu(bus);
  bus.Load(0x01000, Join({kRelocatePcb,
                          OutWord(0x0038, 0x0000),  // INT0 control: edge, unmasked, priority 0
                          {0xFB, 0xF4, 0xEB, 0xFD}}));  // sti; hlt; jmp hlt
  bus.Vector(0x0C, 0x0000, 0x0600);
  bus.Load(0x00600, Join({{0xFE, 0xC3},                 // inc bl
                          OutWord(0x0022, 0x000C),      // specific EOI, type 0Ch
                          {0xCF}}));                    // iret
  cpu.SetRegisters(At(0x0100, 0x0000));
  Check(RunUntilHalt(cpu), "INT0 program halts");
  for (int request = 0; request < 2; ++request) {
    cpu.SetInterruptPin(0, true);
    cpu.Run(2000);
    cpu.SetInterruptPin(0, false);
  }
  Check((cpu.GetRegisters().bx & 0xFF) == 2, "INT0 twice through the PCB, EOI clears in-service");
}

void TestTimerInterrupt() {
  // Timer 0 continuous, max count 100: an interrupt every 400 CPU cycles
  // (timers count every fourth cycle, i186.cpp restart_timer). The CPU
  // waits in HLT, so this also checks that HLT skips to the timer event.
  Bus bus;
  Cpu80188 cpu(bus);
  bus.Load(0x01000, Join({kRelocatePcb,
                          OutWord(0x0052, 100),     // T0 max count A
                          OutWord(0x0032, 0x0000),  // timer interrupt control: unmasked
                          OutWord(0x0056, 0xE001),  // T0 control: EN, INH, INT, CONT
                          {0xFB, 0xF4, 0xEB, 0xFD}}));
  bus.Vector(0x08, 0x0000, 0x0700);
  bus.Load(0x00700, Join({{0x41},                       // inc cx
                          OutWord(0x0022, 0x0008),      // specific EOI, type 08h
                          {0xCF}}));
  cpu.SetRegisters(At(0x0100, 0x0000));
  Check(RunUntilHalt(cpu), "timer program halts");
  const uint16_t before = cpu.GetRegisters().cx;
  cpu.Run(400 * 50);
  const int ticks = cpu.GetRegisters().cx - before;
  if (ticks < 49 || ticks > 51) std::cout << "  timer ticks: " << ticks << "\n";
  Check(ticks >= 49 && ticks <= 51, "timer 0 interrupts every 400 cycles");
}

void TestDma() {
  Bus bus;
  Cpu80188 cpu(bus);
  // Unsynchronised memory to memory: the whole block at once.
  for (int i = 0; i < 16; ++i) bus.memory[0x02000 + i] = static_cast<uint8_t>(i * 3 + 1);
  bus.memory[0x03010] = 0xAA;
  bus.Load(0x01000, Join({kRelocatePcb,
                          OutWord(0x00C0, 0x2000), OutWord(0x00C2, 0),  // source 02000h
                          OutWord(0x00C4, 0x3000), OutWord(0x00C6, 0),  // destination 03000h
                          OutWord(0x00C8, 16),                          // count
                          // DEST mem+inc, SRC mem+inc, TC, START, CHG, byte, no sync
                          OutWord(0x00CA, 0xB606),
                          {0xF4}}));
  cpu.SetRegisters(At(0x0100, 0x0000));
  Check(RunUntilHalt(cpu), "DMA program halts");
  bool copied = bus.memory[0x03010] == 0xAA;
  for (int i = 0; i < 16; ++i) copied = copied && bus.memory[0x03000 + i] == bus.memory[0x02000 + i];
  Check(copied, "DMA0 copies 16 bytes memory to memory");

  // Destination-synchronised memory to a fixed port, the shape SPEAK.EXE
  // programs (HANDOFF 6.3): one byte per DRQ0, stop at terminal count.
  Bus bus2;
  Cpu80188 cpu2(bus2);
  for (int i = 0; i < 4; ++i) bus2.memory[0x02000 + i] = static_cast<uint8_t>(0x10 + i);
  bus2.Load(0x01000, Join({kRelocatePcb,
                           OutWord(0x00C0, 0x2000), OutWord(0x00C2, 0),
                           OutWord(0x00C4, 0x0401), OutWord(0x00C6, 0),  // port 0401h
                           OutWord(0x00C8, 4),
                           // DEST I/O no change, SRC mem+inc, TC, sync dest, START, CHG
                           OutWord(0x00CA, 0x7686),
                           {0xEB, 0xFE}}));  // jmp $
  cpu2.SetRegisters(At(0x0100, 0x0000));
  cpu2.Run(1000);
  Check(bus2.writes.empty(), "synchronised DMA waits for DRQ");
  cpu2.SetDmaRequest(0, true);
  cpu2.Run(1000);
  const std::vector<std::pair<uint16_t, uint8_t>> expected = {
      {0x0401, 0x10}, {0x0401, 0x11}, {0x0401, 0x12}, {0x0401, 0x13}};
  if (bus2.writes != expected) {
    std::cout << "  DMA writes:";
    for (const auto& [port, value] : bus2.writes) std::cout << " " << std::hex << port << "=" << int(value) << std::dec;
    std::cout << "\n";
  }
  Check(bus2.writes == expected, "DRQ0 moves 4 bytes to port 0401h and stops at terminal count");
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
  TestExternalInterrupt();
  TestTimerInterrupt();
  TestDma();
  TestEscapeTrap();
  std::cout << (failures == 0 ? "PASS" : "FAIL") << " cpu_selftest\n";
  return failures == 0 ? 0 : 1;
}
