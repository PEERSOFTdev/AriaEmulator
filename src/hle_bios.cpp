#include "hle_bios.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "aria_machine.h"

namespace {

// Layout of the 1 KB ROM stub at FFC0:0000 (FFC00h-FFFFFh).
constexpr uint16_t kIret = 0x0000;         // Default handler: IRET.
constexpr uint16_t kStubs = 0x0010;        // 4 bytes each: STI; 0Fh nn; IRET.
constexpr uint16_t kTick = 0x0100;         // INT 08h, the tick as software calls it.
constexpr uint16_t kTickIrq = 0x0120;      // INT 0Eh (INT2): the hardware tick.
constexpr uint16_t kDiskTable = 0x0180;    // INT 1Eh: diskette parameters.
constexpr uint16_t kResetVector = 0x03F0;  // FFFF:0000.
constexpr uint16_t kModelByte = 0x03FE;    // FFFF:000Eh, read by AAPI.EXE+46DEh.

// BIOS interrupts with a C++ handler (HANDOFF 6.1: what B: and FreeDOS call).
constexpr uint8_t kHandled[] = {0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1A};

// The trap PatchKernel puts in place of the kernel's allocation of its
// resident code; no interrupt has this number, only our 0Fh F0h.
constexpr uint8_t kKernelRomTrap = 0xF0;

// configDone() in the unpacked FreeDOS ke2046 kernel (config.c, compiled
// by Watcom; seen at 026B:ADFD): hma_paras = (HMAFree + 15) / 16 in AX,
// kernel_seg = allocmem(AX), the MCB below it named "SC" with owner 8, then
// MoveKernel(kernel_seg). -1 stands for any byte. The allocmem call is at
// kAllocCall, the MoveKernel call at kMoveCall.
constexpr int kConfigDone[] = {0xA1, -1,   -1,   0x05, 0x0F, 0x00, 0xB1, 0x04, 0xD3, 0xE8, 0x50, 0xE8,
                               -1,   -1,   0x31, 0xDB, 0x89, 0xC2, 0x4A, 0x8E, 0xC2, 0x26, 0xC7, 0x47,
                               0x08, 0x53, 0x43, 0x26, 0xC7, 0x47, 0x01, 0x08, 0x00, 0xE8, -1,   -1};
constexpr std::size_t kAllocCall = 11, kMoveCall = 33;
// MoveKernel's prologue down to "mov si,[CurrentKernelSegment]; test si,si"
// (inithma.c); the variable's DS offset is at kSegmentVar.
constexpr int kMoveKernel[] = {0x53, 0x51, 0x52, 0x56, 0x57, 0x06, 0x55, 0x89, 0xE5, 0x83,
                               0xEC, -1,   0x89, 0xC1, 0x8B, 0x36, -1,   -1,   0x85, 0xF6};
constexpr std::size_t kSegmentVar = 16;

// The PCB after the BIOS moved it to I/O 0000h, as on the Aria (HANDOFF 6.1).
constexpr uint16_t kPcbRelocation = 0x00FE;  // Offset 7Fh at its new base.
constexpr uint16_t kTimer0MaxA = 0x0052, kTimer0Control = 0x0056;
constexpr uint16_t kTimer2MaxA = 0x0062, kTimer2Control = 0x0066;
constexpr uint16_t kEoi = 0x0022, kInService = 0x002C;
constexpr uint16_t kTimerIntControl = 0x0032;
constexpr uint16_t kInt2Control = 0x003C;
constexpr uint16_t kUmcs = 0x00A0;

// BIOS data area at 0040:0000.
constexpr uint32_t kBda = 0x400;
constexpr uint16_t kEquipment = 0x10, kMemorySize = 0x13, kKeyFlags = 0x17;
constexpr uint16_t kDiskStatus = 0x41, kVideoMode = 0x49, kColumns = 0x4A, kPageSize = 0x4C;
constexpr uint16_t kCursor = 0x50, kCursorShape = 0x60, kTicks = 0x6C, kMidnight = 0x70;
constexpr uint16_t kRows = 0x84;

}  // namespace

// Register view with the 8-bit halves BIOS calls use.
struct HleBios::Regs {
  Cpu80188::Registers r;
  uint8_t ah() const { return static_cast<uint8_t>(r.ax >> 8); }
  uint8_t al() const { return static_cast<uint8_t>(r.ax); }
  uint8_t bh() const { return static_cast<uint8_t>(r.bx >> 8); }
  uint8_t bl() const { return static_cast<uint8_t>(r.bx); }
  uint8_t ch() const { return static_cast<uint8_t>(r.cx >> 8); }
  uint8_t cl() const { return static_cast<uint8_t>(r.cx); }
  uint8_t dh() const { return static_cast<uint8_t>(r.dx >> 8); }
  uint8_t dl() const { return static_cast<uint8_t>(r.dx); }
  void set_ah(uint8_t v) { r.ax = static_cast<uint16_t>((r.ax & 0x00FF) | v << 8); }
  void set_al(uint8_t v) { r.ax = static_cast<uint16_t>((r.ax & 0xFF00) | v); }
  void set_bl(uint8_t v) { r.bx = static_cast<uint16_t>((r.bx & 0xFF00) | v); }
  void set_ch(uint8_t v) { r.cx = static_cast<uint16_t>((r.cx & 0x00FF) | v << 8); }
  void set_cl(uint8_t v) { r.cx = static_cast<uint16_t>((r.cx & 0xFF00) | v); }
  void set_dh(uint8_t v) { r.dx = static_cast<uint16_t>((r.dx & 0x00FF) | v << 8); }
  void set_dl(uint8_t v) { r.dx = static_cast<uint16_t>((r.dx & 0xFF00) | v); }
  bool wait = false;  // Re-run the stub: the call is waiting (INT 16h).
};

HleBios::HleBios(AriaMachine& machine) : machine_(machine) {}

void HleBios::PowerOn(const std::vector<uint8_t>& kernel) {
  uint8_t* rom = machine_.RomStub();
  std::memset(rom, 0xFF, 1024);
  rom[kIret] = 0xCF;
  for (std::size_t i = 0; i < sizeof kHandled; ++i) {
    uint8_t* stub = rom + kStubs + i * 4;
    stub[0] = 0xFB;  // STI, as BIOS handlers do: a wait must not block the tick.
    stub[1] = 0x0F;
    stub[2] = kHandled[i];
    stub[3] = 0xCF;
  }
  // The tick reaches the INT 08h chain only after the hardware interrupt is
  // acknowledged. SPEAK hooks INT 08h and synthesises speech inside it with
  // interrupts on (SPEAK.EXE resident 8A6Ch -> 8A05h -> 7FF0h), where it can
  // wait for its DMA 0 interrupt (62A0h); SPEAK gives that interrupt
  // priority 7 (+0F86h, +62BBh, +780Eh), so a timer interrupt still in
  // service would block it for good, at any timer priority. Hence the
  // hardware tick comes in on INT2 (vector 0Eh, used by nothing on B:,
  // tools/ints.py), whose handler acknowledges it and then calls INT 08h
  // in software (HANDOFF 6.1). Assumption: an 18.2 Hz source on the board
  // drives INT2 (AriaMachine::kTickNumerator); the Aria's real wiring is
  // unknown, and no timer of the 80C188 can make that rate at 15 MHz.
  //
  // INT 0Eh: trap 0Eh (clears the board's tick latch), specific EOI for
  // type 0Eh to the PCB at 0022h (offset 11h), INT 08h.
  const uint8_t tick_irq[] = {0x0F, 0x0E,                // trap 0Eh
                              0x50, 0x52,                // push ax, dx
                              0xBA, 0x22, 0x00,          // mov dx,0022h
                              0xB8, 0x0E, 0x00,          // mov ax,000Eh
                              0xEF,                      // out dx,ax
                              0x5A, 0x58,                // pop dx, ax
                              0xCD, 0x08,                // int 08h
                              0xCF};                     // iret
  std::memcpy(rom + kTickIrq, tick_irq, sizeof tick_irq);
  // INT 08h: count the tick in C++, then the user hook INT 1Ch.
  const uint8_t tick[] = {0x0F, 0x08,                    // trap 08h
                          0xCD, 0x1C,                    // int 1Ch
                          0xCF};                         // iret
  std::memcpy(rom + kTick, tick, sizeof tick);
  // 1.44 MB diskette parameters (IBM); FreeDOS patches byte 4, sectors per
  // track, before a CHS transfer (dsk.c LBA_Transfer).
  const uint8_t table[] = {0xAF, 0x02, 0x25, 0x02, 0x12, 0x1B, 0xFF, 0x6C, 0xF6, 0x0F, 0x08};
  std::memcpy(rom + kDiskTable, table, sizeof table);
  rom[kResetVector] = 0xF4;  // HLT: power-on is done in C++, see below.
  // Model byte at FFFF:000Eh. The Aria's value is unknown (HANDOFF 6.5);
  // FCh is an AT, the closest PC class for an 80186 machine.
  rom[kModelByte] = 0xFC;

  Cpu80188& cpu = machine_.cpu();
  // Move the PCB to I/O 0000h (HANDOFF 6.1) with the ESC trap on.
  // Assumption: VOICE.SYS+01CEh installs an INT 7 handler that skips ESC
  // instructions, which only makes sense if the boot ROM set the trap; our
  // INT 07h does the same until it loads. FF00h is where the PCB sits at
  // reset.
  cpu.OutWord(0xFFFE, 0x8000);
  // Timer 0 runs free with a 1 ms period and no interrupt: AAPI's delay
  // counts its periods, each by its count passing 753h, half of 3750
  // (AAPI.EXE+10B35, called with 100 and 6 at +DE0C and +F5EC); 3750 at
  // the timers' 3.75 MHz is 1 ms (HANDOFF 6.4). The tick comes from the
  // board on INT2 (aria_machine.cpp). Timer 1 is FOSSIL's (FOSSIL.COM+07EB).
  cpu.OutWord(kTimer0MaxA, 3750);
  cpu.OutWord(kTimer0Control, 0xC001);    // EN, INH, CONT; no interrupt.
  cpu.OutWord(kInt2Control, 0x0000);      // INT2: unmasked, edge, priority 0.
  // Timer 2 interrupts periodically on vector 13h, the 80186's type for it:
  // on the Aria "INT 13h" is this hardware interrupt, and BKB scans the key
  // matrix in its hook (BKB.COM+1A40h, call 02E5h). AAPI's hook on the same
  // vector reloads timer 2's maximum with C925h before passing it on
  // (AAPI.EXE+9F20), so the BIOS starts with that value: 3.75 MHz /
  // 51493 = 72.8 scans a second (HANDOFF 6.4). The vector is shared with
  // the disk service FreeDOS needs; the INT 13h handler tells them apart.
  cpu.OutWord(kTimer2MaxA, 0xC925);
  cpu.OutWord(kTimer2Control, 0xE001);    // EN, INH, INT, CONT.
  cpu.OutWord(kTimerIntControl, 0x0000);  // Timer interrupts unmasked, priority 0.
  // Upper chip select from F0000h, so the ROM area holding the kernel's
  // code covers the top of the RAM disk's memory, as the Aria's ROM did
  // (AriaMachine::kRomArea; RAMDISK.EXE saves UMCS, shrinks it to FFF8h for
  // each access and restores it, +0372h-+03A9h). Bits 5-3 read as ones;
  // no wait states. The Aria's own value is unknown [assumption].
  cpu.OutWord(kUmcs, static_cast<uint16_t>((AriaMachine::kRomArea >> 10) << 6 | 0x38));

  // Vector table: everything to IRET, then the handlers.
  for (int n = 0; n < 256; ++n) {
    machine_.PokeWord(n * 4u, kIret);
    machine_.PokeWord(n * 4u + 2, kSegment);
  }
  for (std::size_t i = 0; i < sizeof kHandled; ++i) {
    machine_.PokeWord(kHandled[i] * 4u, static_cast<uint16_t>(kStubs + i * 4));
  }
  machine_.PokeWord(0x08 * 4u, kTick);
  machine_.PokeWord(0x0E * 4u, kTickIrq);
  machine_.PokeWord(0x1E * 4u, kDiskTable);

  // BIOS data area.
  for (uint32_t a = kBda; a < kBda + 0x100; ++a) machine_.Poke(a, 0);
  // Equipment: 2 floppy-type drives (A: ROM disk, B: flash), 80x25 colour,
  // one serial port, one printer port. The printer port is LPT1 at 680h:
  // BACKPACK.SYS takes its port from here (file offset +32FDh) and talks
  // only to 680h-682h (+18DAh, +18FCh), so the boot ROM had put 680h here
  // (parallel_port.h, HANDOFF 6.7).
  machine_.PokeWord(kBda + kEquipment, 0x4261);
  machine_.PokeWord(kBda + 0x08, ParallelPort::kBase);
  // 704 KB for DOS, up to the RAM disk D: at B0000h (HANDOFF 6.2). Nothing
  // on B: names the size, but the 1 MB RAM divides into exactly this, the
  // 64 KB at A0000h, the RAM disk's 319 KB and the 1 KB of boot ROM; at
  // 640 KB those 64 KB would lie unused [assumption]. With the kernel's
  // code in ROM it leaves the word processor the room it had on the Aria
  // (HANDOFF 6.2, 9. 10. 2026).
  machine_.PokeWord(kBda + kMemorySize, 704);
  machine_.Poke(kBda + kVideoMode, 3);
  machine_.PokeWord(kBda + kColumns, 80);
  machine_.PokeWord(kBda + kPageSize, 4000);
  machine_.PokeWord(kBda + kCursorShape, 0x0607);
  machine_.Poke(kBda + kRows, 24);
  std::memset(screen_, 0, sizeof screen_);
  for (auto& row : screen_) for (auto& cell : row) { cell[0] = ' '; cell[1] = 0x07; }
  cursor_row_ = cursor_col_ = 0;
  teletype_.clear();
  keys_.clear();
  reboot_ = false;
  kernel_patch_tried_ = false;
  kernel_in_rom_ = false;

  // FreeDOS boot hand-off (boot.asm): KERNEL.SYS at 0060:0000, entered
  // far with the BIOS boot drive in BL. Booting from B: makes the kernel
  // read the real CONFIG.SYS there. No boot sector at 0000:7C00: the
  // kernel (packed 0600h-BEDAh, unpacked up to 12E1Ch) covers that place,
  // and the kernel itself only jumps there for "boot from another disk"
  // (main.c). The entry pushes BP on the caller's stack, so the stack sits
  // below the kernel; BP = 0 tells it no command line was passed
  // (kernel.asm initialise_command_line_buffer).
  for (std::size_t i = 0; i < kernel.size() && 0x600 + i < 0xA0000; ++i) {
    machine_.Poke(static_cast<uint32_t>(0x600 + i), kernel[i]);
  }
  // The kernel's configuration block ("CONFIG" at offset 2, kernel.asm
  // _LowKernelConfig, the fields FreeDOS's SYS CONFIG sets): SkipConfigSeconds
  // at offset 12 is the "Press F8 to trace or F5 to skip" pause, 2 s by
  // default. No key can answer it here, so the copy in memory gets 0.
  if (kernel.size() > 16 && std::memcmp(kernel.data() + 2, "CONFIG", 6) == 0) {
    machine_.Poke(0x600 + 12, 0);
  }
  Cpu80188::Registers r;
  r.cs = 0x0060;
  r.ip = 0x0000;
  r.bx = 0x0001;
  r.dx = 0x0001;
  r.ss = 0x0000;
  r.sp = 0x0600;
  r.bp = 0x0000;
  r.flags = 0xF202;
  cpu.SetRegisters(r);
}

void HleBios::SetCallerCarry(bool carry) {
  // The stub's IRET restores the caller's FLAGS from SS:SP+4.
  const Cpu80188::Registers r = machine_.cpu().GetRegisters();
  const uint32_t at = (static_cast<uint32_t>(r.ss) << 4) + static_cast<uint16_t>(r.sp + 4);
  uint16_t flags = machine_.PeekWord(at);
  flags = static_cast<uint16_t>(carry ? flags | 0x0001 : flags & ~0x0001);
  machine_.PokeWord(at, flags);
}

void HleBios::SetCallerZero(bool zero) {
  const Cpu80188::Registers r = machine_.cpu().GetRegisters();
  const uint32_t at = (static_cast<uint32_t>(r.ss) << 4) + static_cast<uint16_t>(r.sp + 4);
  uint16_t flags = machine_.PeekWord(at);
  flags = static_cast<uint16_t>(zero ? flags | 0x0040 : flags & ~0x0040);
  machine_.PokeWord(at, flags);
}

// The FreeDOS kernel keeps its resident code (the HMA_TEXT group, 40.6 KB
// in ke2046) in the HMA when it can, and otherwise in a block of DOS memory
// named "SC" (config.c configDone). The 80C188 has no HMA, and ROM-DOS ran
// from the boot ROM, so on the Aria that memory was the programs': without
// it the word processor (AWP.EXE, 249 KB) does not start and MAINMENU says
// "plná paměť" (HANDOFF 6.2, 9. 10. 2026). So the allocation becomes a trap
// that copies the code into the ROM area and hands MoveKernel that segment;
// MoveKernel then points the kernel's far jumps there. It copies only
// downwards itself (inithma.c), hence the copy here. Only an exact match of
// configDone and MoveKernel is patched; anything else leaves the kernel as
// it is, and boot_test notices the "SC" block.
void HleBios::PatchKernel() {
  kernel_patch_tried_ = true;
  auto matches = [this](uint32_t at, const int* pattern, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
      if (pattern[i] >= 0 && machine_.Peek(at + static_cast<uint32_t>(i)) != pattern[i]) return false;
    }
    return true;
  };
  uint32_t site = 0;
  int found = 0;
  for (uint32_t at = 0x600; at + std::size(kConfigDone) < 0xA0000; ++at) {
    if (matches(at, kConfigDone, std::size(kConfigDone))) {
      site = at;
      ++found;
    }
  }
  if (found != 1) return;
  const uint32_t after_call = site + kMoveCall + 3;
  const uint32_t move_kernel = after_call + static_cast<int16_t>(machine_.PeekWord(site + kMoveCall + 1));
  if (!matches(move_kernel, kMoveKernel, std::size(kMoveKernel))) return;
  kernel_segment_var_ = machine_.PeekWord(move_kernel + kSegmentVar);
  machine_.Poke(site + kAllocCall, 0x0F);
  machine_.Poke(site + kAllocCall + 1, kKernelRomTrap);
  machine_.Poke(site + kAllocCall + 2, 0x90);  // NOP for the third byte of the call.
}

void HleBios::KernelToRom(Regs& r) {
  // AX = paragraphs of resident code; the code sits at CurrentKernelSegment
  // :0000 (MoveKernel copies from offset HMATextStart & FFF0h, which is 0).
  // The returned segment is one paragraph into the ROM area, so the "SC"
  // MCB configDone writes below it lands in the area too, not on D:.
  const uint32_t bytes = static_cast<uint32_t>(r.r.ax) * 16;
  const uint16_t source = machine_.PeekWord((static_cast<uint32_t>(r.r.ds) << 4) + kernel_segment_var_);
  const uint16_t target = static_cast<uint16_t>((AriaMachine::kRomArea >> 4) + 1);
  if ((static_cast<uint32_t>(target) << 4) + bytes > AriaMachine::kRomStub) {
    // Cannot happen with ke2046 (9EB0h bytes in 63 KB), but a kernel that
    // grew must not overwrite the BIOS: stop where it is visible.
    std::fprintf(stderr, "kernel code %u bytes does not fit the ROM area\n", static_cast<unsigned>(bytes));
    r.r.ax = 0;
    return;
  }
  for (uint32_t i = 0; i < bytes; ++i) {
    machine_.Poke((static_cast<uint32_t>(target) << 4) + i, machine_.Peek((static_cast<uint32_t>(source) << 4) + i));
  }
  r.r.ax = target;
  kernel_in_rom_ = true;
}

void HleBios::Trap(uint8_t number) {
  Regs regs{machine_.cpu().GetRegisters()};
  switch (number) {
    case 0x07: {
      // ESC trap (80186, bit 15 of the relocation register): skip the ESC
      // opcode and its modrm like VOICE.SYS's handler does later
      // (VOICE.SYS+01C5h); no coprocessor exists.
      const uint32_t at = (static_cast<uint32_t>(regs.r.ss) << 4) + regs.r.sp;
      machine_.PokeWord(at, static_cast<uint16_t>(machine_.PeekWord(at) + 2));
      return;
    }
    case 0x08: TimerTick(); return;
    case 0x0E: machine_.AcknowledgeTick(); return;
    case 0x10: Video(regs); break;
    case 0x11: regs.r.ax = machine_.PeekWord(kBda + kEquipment); break;
    case 0x12:
      regs.r.ax = machine_.PeekWord(kBda + kMemorySize);
      // The kernel asks once, unpacked by then (main.c init_oem) and before
      // CONFIG.SYS: the moment to patch it.
      if (!kernel_patch_tried_) PatchKernel();
      break;
    case 0x13:
      // A timer interrupt in service means this is timer 2's tick (above),
      // not a disk call: acknowledge it and leave the registers alone.
      // Software calls never come from inside a timer interrupt here: the
      // tick's INT 08h chain runs on INT2 (HANDOFF 6.1).
      if (machine_.cpu().InWord(kInService) & 0x0001) {
        machine_.cpu().OutWord(kEoi, 0x0008);
        return;
      }
      Disk(regs);
      break;
    case 0x14: Serial(regs); break;
    case 0x15: System(regs); break;
    case 0x16: Keyboard(regs); break;
    case 0x17: Printer(regs); break;
    case 0x19: reboot_ = true; regs.wait = true; break;
    case 0x1A: Time(regs); break;
    case kKernelRomTrap: KernelToRom(regs); break;
    default: return;
  }
  if (regs.wait) regs.r.ip = static_cast<uint16_t>(regs.r.ip - 3);  // Back to STI; 0Fh nn.
  machine_.cpu().SetRegisters(regs.r);
}

void HleBios::TimerTick() {
  uint32_t ticks = machine_.PeekWord(kBda + kTicks) | static_cast<uint32_t>(machine_.PeekWord(kBda + kTicks + 2)) << 16;
  if (++ticks >= 0x1800B0) {  // 24 hours of ticks.
    ticks = 0;
    machine_.Poke(kBda + kMidnight, 1);
  }
  machine_.PokeWord(kBda + kTicks, static_cast<uint16_t>(ticks));
  machine_.PokeWord(kBda + kTicks + 2, static_cast<uint16_t>(ticks >> 16));
}

// ---- INT 10h: an 80x25 text screen nobody sees; AAPI turns AH=0Eh into
// speech (API.DOC) and its virtual screen reads the text back. ----

void HleBios::SetCursor(int row, int col) {
  cursor_row_ = std::clamp(row, 0, 24);
  cursor_col_ = std::clamp(col, 0, 79);
  machine_.Poke(kBda + kCursor, static_cast<uint8_t>(cursor_col_));
  machine_.Poke(kBda + kCursor + 1, static_cast<uint8_t>(cursor_row_));
}

void HleBios::Scroll(int top, int left, int bottom, int right, int lines, uint8_t attr, bool up) {
  top = std::clamp(top, 0, 24);
  bottom = std::clamp(bottom, 0, 24);
  left = std::clamp(left, 0, 79);
  right = std::clamp(right, 0, 79);
  const int height = bottom - top + 1;
  if (lines == 0 || lines > height) lines = height;
  for (int i = 0; i < height; ++i) {
    const int row = up ? top + i : bottom - i;
    const int from = up ? row + lines : row - lines;
    for (int col = left; col <= right; ++col) {
      if (from >= top && from <= bottom) {
        screen_[row][col][0] = screen_[from][col][0];
        screen_[row][col][1] = screen_[from][col][1];
      } else {
        screen_[row][col][0] = ' ';
        screen_[row][col][1] = attr;
      }
    }
  }
}

void HleBios::PutChar(uint8_t ch) {
  teletype_.push_back(static_cast<char>(ch));
  switch (ch) {
    case 0x07: return;  // BEL.
    case 0x08: if (cursor_col_ > 0) --cursor_col_; break;
    case 0x0A: ++cursor_row_; break;
    case 0x0D: cursor_col_ = 0; break;
    default:
      screen_[cursor_row_][cursor_col_][0] = ch;
      if (++cursor_col_ >= 80) {
        cursor_col_ = 0;
        ++cursor_row_;
      }
      break;
  }
  if (cursor_row_ >= 25) {
    Scroll(0, 0, 24, 79, 1, 0x07, true);
    cursor_row_ = 24;
  }
  SetCursor(cursor_row_, cursor_col_);
}

void HleBios::Video(Regs& r) {
  switch (r.ah()) {
    case 0x00:  // Set mode: clear.
      Scroll(0, 0, 24, 79, 0, 0x07, true);
      SetCursor(0, 0);
      break;
    case 0x01: machine_.PokeWord(kBda + kCursorShape, r.r.cx); break;
    case 0x02: SetCursor(r.dh(), r.dl()); break;
    case 0x03:
      r.set_dh(static_cast<uint8_t>(cursor_row_));
      r.set_dl(static_cast<uint8_t>(cursor_col_));
      r.r.cx = machine_.PeekWord(kBda + kCursorShape);
      break;
    case 0x05: break;  // One page only.
    case 0x06: Scroll(r.ch(), r.cl(), r.dh(), r.dl(), r.al(), r.bh(), true); break;
    case 0x07: Scroll(r.ch(), r.cl(), r.dh(), r.dl(), r.al(), r.bh(), false); break;
    case 0x08:
      r.set_al(screen_[cursor_row_][cursor_col_][0]);
      r.set_ah(screen_[cursor_row_][cursor_col_][1]);
      break;
    case 0x09:
    case 0x0A:
      for (int i = 0, row = cursor_row_, col = cursor_col_; i < r.r.cx; ++i) {
        screen_[row][col][0] = r.al();
        if (r.ah() == 0x09) screen_[row][col][1] = r.bl();
        if (++col >= 80) {
          col = 0;
          if (++row >= 25) break;
        }
      }
      break;
    case 0x0E: PutChar(r.al()); break;
    case 0x0F:
      r.set_al(3);
      r.set_ah(80);
      r.r.bx = static_cast<uint16_t>(r.r.bx & 0x00FF);  // BH = page 0.
      break;
    default: break;
  }
}

std::string HleBios::ScreenLine(int row) const {
  std::string line;
  for (int col = 0; col < 80; ++col) line.push_back(static_cast<char>(screen_[row][col][0]));
  while (!line.empty() && line.back() == ' ') line.pop_back();
  return line;
}

// ---- INT 13h: A: ROM disk and B: flash image (HANDOFF 6.11). ----

void HleBios::Disk(Regs& r) {
  const uint8_t drive = r.dl();
  const Regs in = r;
  auto finish = [&](uint8_t status) {
    if (disk_log_) {
      char line[96];
      std::snprintf(line, sizeof line, "INT 13h AX=%04X CX=%04X DX=%04X ES:BX=%04X:%04X -> AH=%02X AL=%02X",
                    in.r.ax, in.r.cx, in.r.dx, in.r.es, in.r.bx, status, r.al());
      disk_log_(line);
    }
    r.set_ah(status);
    machine_.Poke(kBda + kDiskStatus, status);
    SetCallerCarry(status != 0);
  };
  if (drive > 1) {  // No hard disks.
    finish(0x01);
    return;
  }
  BiosDisk& disk = machine_.disk(drive);
  switch (r.ah()) {
    case 0x00: finish(0); return;  // Reset.
    case 0x01: r.set_al(machine_.Peek(kBda + kDiskStatus)); SetCallerCarry(false); return;
    case 0x02:  // Read.
    case 0x03:  // Write.
    case 0x04: {  // Verify.
      const int cylinder = r.ch() | (r.cl() & 0xC0) << 2;
      const int sector = r.cl() & 0x3F;
      const int head = r.dh();
      if (sector == 0 || sector > disk.sectors_per_track || head >= disk.heads) {
        finish(0x04);  // Sector not found.
        return;
      }
      uint32_t lba = (static_cast<uint32_t>(cylinder) * disk.heads + head) * disk.sectors_per_track + sector - 1;
      uint32_t buffer = (static_cast<uint32_t>(r.r.es) << 4) + r.r.bx;
      int done = 0;
      for (; done < r.al(); ++done, ++lba, buffer += 512) {
        if (lba >= disk.sectors || (lba + 1) * 512 > disk.data.size()) break;
        uint8_t* sector_data = disk.data.data() + lba * 512;
        if (r.ah() == 0x02) {
          for (int i = 0; i < 512; ++i) machine_.Poke(buffer + i, sector_data[i]);
          if (lba == 0 && disk.patch_boot_record) {
            machine_.PokeWord(buffer + 0x18, disk.sectors_per_track);
            machine_.PokeWord(buffer + 0x1A, disk.heads);
            // DLRDISK leaves 1FEh-1FFh unprogrammed or random (59h 1Fh in
            // 7.05, 47h 63h in 7.04); FreeDOS ignores a BPB without 55AAh
            // (dsk.c getbpb) and would assume a 1.44 MB diskette.
            machine_.Poke(buffer + 0x1FE, 0x55);
            machine_.Poke(buffer + 0x1FF, 0xAA);
          }
        } else if (r.ah() == 0x03) {
          for (int i = 0; i < 512; ++i) sector_data[i] = machine_.Peek(buffer + i);
        }
      }
      r.set_al(static_cast<uint8_t>(done));
      finish(done == 0 && r.al() != 0 ? 0x04 : 0);
      return;
    }
    case 0x08: {  // Drive parameters.
      const int cylinders = static_cast<int>((disk.sectors + disk.sectors_per_track * disk.heads - 1) /
                                             (disk.sectors_per_track * disk.heads));
      const int max_cyl = cylinders - 1;
      r.set_bl(4);  // "1.44 MB": the BPB on the disk overrides it (FreeDOS initdisk.c).
      r.set_ch(static_cast<uint8_t>(max_cyl));
      r.set_cl(static_cast<uint8_t>((disk.sectors_per_track & 0x3F) | (max_cyl >> 2 & 0xC0)));
      r.set_dh(static_cast<uint8_t>(disk.heads - 1));
      r.set_dl(2);
      r.r.es = kSegment;
      r.r.di = kDiskTable;
      r.r.ax = 0;
      finish(0);
      return;
    }
    case 0x15: r.set_ah(0x01); SetCallerCarry(false); return;  // Floppy, no change line.
    case 0x16: finish(0); return;                              // Disk not changed.
    default: finish(0x01); return;                             // Includes 41h: no LBA.
  }
}

// ---- INT 16h until BKB.COM replaces it (BKB.COM+19F9h). ----

void HleBios::Keyboard(Regs& r) {
  switch (r.ah()) {
    case 0x00:
    case 0x10:
      if (keys_.empty()) {
        r.wait = true;
        return;
      }
      r.r.ax = keys_.front();
      keys_.pop_front();
      return;
    case 0x01:
    case 0x11:
      if (keys_.empty()) {
        SetCallerZero(true);
      } else {
        r.r.ax = keys_.front();
        SetCallerZero(false);
      }
      return;
    case 0x02:
    case 0x12:
      r.set_al(machine_.Peek(kBda + kKeyFlags));
      return;
    case 0x05:
      keys_.push_back(r.r.cx);
      r.set_al(0);
      return;
    default: return;
  }
}

void HleBios::Serial(Regs& r) {
  // Until FOSSIL.COM takes over INT 14h (HANDOFF 6.6): no line attached.
  switch (r.ah()) {
    case 0x00: case 0x03: r.r.ax = 0x6000; return;  // Transmitter empty, no data.
    case 0x01: r.set_ah(0x00); return;
    case 0x02: r.set_ah(0x80); return;              // Time-out.
    default: return;
  }
}

void HleBios::System(Regs& r) {
  switch (r.ah()) {
    case 0x88: r.r.ax = 0; SetCallerCarry(false); return;   // No extended memory.
    case 0x90: case 0x91: r.set_ah(0); SetCallerCarry(false); return;
    default: r.set_ah(0x86); SetCallerCarry(true); return;  // Unsupported.
  }
}

void HleBios::Printer(Regs& r) {
  // No printer yet (HANDOFF 6.7): report "not busy, selected".
  r.set_ah(0x90);
}

// ---- INT 1Ah: tick count and the MC146818 (HANDOFF 6.5). ----

void HleBios::Time(Regs& r) {
  Rtc146818& rtc = machine_.rtc();
  switch (r.ah()) {
    case 0x00:
      r.r.cx = machine_.PeekWord(kBda + kTicks + 2);
      r.r.dx = machine_.PeekWord(kBda + kTicks);
      r.set_al(machine_.Peek(kBda + kMidnight));
      machine_.Poke(kBda + kMidnight, 0);
      return;
    case 0x01:
      machine_.PokeWord(kBda + kTicks + 2, r.r.cx);
      machine_.PokeWord(kBda + kTicks, r.r.dx);
      machine_.Poke(kBda + kMidnight, 0);
      return;
    case 0x02:
      r.set_ch(rtc.Read(0x04));
      r.set_cl(rtc.Read(0x02));
      r.set_dh(rtc.Read(0x00));
      r.set_dl(0);
      SetCallerCarry(false);
      return;
    case 0x03:
      rtc.Write(0x04, r.ch());
      rtc.Write(0x02, r.cl());
      rtc.Write(0x00, r.dh());
      SetCallerCarry(false);
      return;
    case 0x04: {
      const uint8_t year = rtc.Read(0x09);
      r.set_ch(year < 0x80 ? 0x20 : 0x19);  // Century: BCD, 1980-2079.
      r.set_cl(year);
      r.set_dh(rtc.Read(0x08));
      r.set_dl(rtc.Read(0x07));
      SetCallerCarry(false);
      return;
    }
    case 0x05:
      rtc.Write(0x09, r.cl());
      rtc.Write(0x08, r.dh());
      rtc.Write(0x07, r.dl());
      SetCallerCarry(false);
      return;
    default: SetCallerCarry(true); return;
  }
}
