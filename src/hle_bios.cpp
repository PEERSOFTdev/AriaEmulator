#include "hle_bios.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "aria_machine.h"

namespace {

// Layout of the 1 KB ROM stub at FFC0:0000 (FFC00h-FFFFFh).
constexpr uint16_t kIret = 0x0000;         // Default handler: IRET.
constexpr uint16_t kStubs = 0x0010;        // 4 bytes each: STI; 0Fh nn; IRET.
constexpr uint16_t kTick = 0x0100;         // INT 08h, the timer tick.
constexpr uint16_t kDiskTable = 0x0180;    // INT 1Eh: diskette parameters.
constexpr uint16_t kResetVector = 0x03F0;  // FFFF:0000.
constexpr uint16_t kModelByte = 0x03FE;    // FFFF:000Eh, read by AAPI.EXE+46DEh.

// BIOS interrupts with a C++ handler (HANDOFF 6.1: what B: and FreeDOS call).
constexpr uint8_t kHandled[] = {0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1A};

// The PCB after the BIOS moved it to I/O 0000h, as on the Aria (HANDOFF 6.1).
constexpr uint16_t kPcbRelocation = 0x00FE;  // Offset 7Fh at its new base.
constexpr uint16_t kTimer0MaxA = 0x0052, kTimer0Control = 0x0056;
constexpr uint16_t kTimerIntControl = 0x0032;

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
  // INT 08h: count the tick in C++, call the user hook INT 1Ch, then a
  // specific EOI for type 08h to the PCB at 0022h (offset 11h).
  const uint8_t tick[] = {0x0F, 0x08,                    // trap 08h
                          0xCD, 0x1C,                    // int 1Ch
                          0x50, 0x52,                    // push ax, dx
                          0xBA, 0x22, 0x00,              // mov dx,0022h
                          0xB8, 0x08, 0x00,              // mov ax,0008h
                          0xEF,                          // out dx,ax
                          0x5A, 0x58,                    // pop dx, ax
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
  // The tick, INT 08h (timer 0 is type 08h on the 80186), from timer 0
  // alone, counting the full 65536: at the assumed 4.77 MHz the timers
  // count 1.19318 MHz, the PC's timer clock, so this is the PC's 18.2 Hz
  // (HANDOFF 6.1). Timer 2 stays free: AAPI's INT 13h hook loads its
  // maximum and reads its count to time disk calls (AAPI.EXE+9F20), and
  // timer 1 is FOSSIL's (FOSSIL.COM+07EB). AAPI's delay loop waits for
  // timer 0's count to pass 753h (AAPI.EXE+10B35), so its maximum is
  // larger than that.
  cpu.OutWord(kTimer0MaxA, 0);            // 0 = 65536.
  cpu.OutWord(kTimer0Control, 0xE001);    // EN, INH, INT, CONT.
  cpu.OutWord(kTimerIntControl, 0x0000);  // Unmasked, priority 0.

  // Vector table: everything to IRET, then the handlers.
  for (int n = 0; n < 256; ++n) {
    machine_.PokeWord(n * 4u, kIret);
    machine_.PokeWord(n * 4u + 2, kSegment);
  }
  for (std::size_t i = 0; i < sizeof kHandled; ++i) {
    machine_.PokeWord(kHandled[i] * 4u, static_cast<uint16_t>(kStubs + i * 4));
  }
  machine_.PokeWord(0x08 * 4u, kTick);
  machine_.PokeWord(0x1E * 4u, kDiskTable);

  // BIOS data area.
  for (uint32_t a = kBda; a < kBda + 0x100; ++a) machine_.Poke(a, 0);
  // Equipment: 2 floppy-type drives (A: ROM disk, B: flash), 80x25 colour,
  // one serial port. The parallel port's PC address is unknown (HANDOFF
  // 6.7), so no LPT in the table yet.
  machine_.PokeWord(kBda + kEquipment, 0x0261);
  // 640 KB for DOS; the RAM disk D: sits above (B0000h, HANDOFF 6.2).
  machine_.PokeWord(kBda + kMemorySize, 640);
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
    case 0x10: Video(regs); break;
    case 0x11: regs.r.ax = machine_.PeekWord(kBda + kEquipment); break;
    case 0x12: regs.r.ax = machine_.PeekWord(kBda + kMemorySize); break;
    case 0x13: Disk(regs); break;
    case 0x14: Serial(regs); break;
    case 0x15: System(regs); break;
    case 0x16: Keyboard(regs); break;
    case 0x17: Printer(regs); break;
    case 0x19: reboot_ = true; regs.wait = true; break;
    case 0x1A: Time(regs); break;
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
