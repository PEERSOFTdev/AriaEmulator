#ifndef ARIA_HLE_BIOS_H
#define ARIA_HLE_BIOS_H

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class AriaMachine;

// The emulator's replacement for the Aria's lost boot ROM BIOS (HANDOFF
// 6.1, 6.10): a stub in the top 1 KB of ROM whose interrupt entries trap
// into C++ (opcode 0Fh nn, i186.cpp), plus the power-on set-up a real BIOS
// does. The scope is what the programs on drive B: and the FreeDOS kernel
// call: INT 10h-1Ah and the timer tick (HANDOFF 6.1).
class HleBios {
 public:
  static constexpr uint16_t kSegment = 0xFFC0;  // Stub at FFC0:0000 = FFC00h.

  explicit HleBios(AriaMachine& machine);

  // Builds the ROM stub, the vector table and the BIOS data area, sets up
  // the PCB, loads the kernel at 0060:0000 and points the CPU at it with
  // the boot drive in BL (HANDOFF 6.10, FreeDOS boot.asm).
  void PowerOn(const std::vector<uint8_t>& kernel);
  void Trap(uint8_t number);

  // Keystrokes for INT 16h before BKB.COM takes the keyboard over:
  // scan code in the high byte, ASCII in the low byte.
  void PushKey(uint16_t key) { keys_.push_back(key); }

  // Everything written through INT 10h AH=0Eh, as bytes in the firmware's
  // encoding; the 80x25 text screen behind INT 10h.
  const std::string& Teletype() const { return teletype_; }
  std::string ScreenLine(int row) const;

  // Diagnostics: one line per INT 13h call, for the boot test.
  void SetDiskLog(std::function<void(const std::string&)> log) { disk_log_ = std::move(log); }

  bool RebootRequested() const { return reboot_; }
  void ClearReboot() { reboot_ = false; }

 private:
  struct Regs;
  void Video(Regs& r);
  void Disk(Regs& r);
  void Keyboard(Regs& r);
  void Serial(Regs& r);
  void System(Regs& r);
  void Printer(Regs& r);
  void Time(Regs& r);
  void TimerTick();

  void PutChar(uint8_t ch);
  void Scroll(int top, int left, int bottom, int right, int lines, uint8_t attr, bool up);
  void SetCursor(int row, int col);
  void SetCallerCarry(bool carry);
  void SetCallerZero(bool zero);

  AriaMachine& machine_;
  std::deque<uint16_t> keys_;
  std::string teletype_;
  uint8_t screen_[25][80][2] = {};
  int cursor_row_ = 0, cursor_col_ = 0;
  bool reboot_ = false;
  std::function<void(const std::string&)> disk_log_;
};

#endif
