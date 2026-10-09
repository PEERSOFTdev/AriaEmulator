#ifndef ARIA_RTC146818_H
#define ARIA_RTC146818_H

#include <cstdint>
#include <functional>

// The Aria's real-time clock: an MC146818/DS12887-compatible chip at I/O
// 0500h + register (HANDOFF 6.5). Registers 00h-09h hold time, date and
// alarm, 0Ah-0Dh are the control registers A-D, the rest is battery-backed
// CMOS RAM up to 7Fh. The clock runs on host time plus an offset that the
// emulated software sets by writing the time registers.
class Rtc146818 {
 public:
  // Seconds since 1970 in local time; injectable for deterministic tests.
  using Clock = std::function<int64_t()>;

  explicit Rtc146818(Clock clock = {});

  uint8_t Read(uint8_t reg);
  void Write(uint8_t reg, uint8_t value);

  // The whole battery-backed chip, for saving and restoring between
  // sessions: all 128 registers and how far the emulated clock is from the
  // host's. The time registers are recomputed on the next read anyway.
  static constexpr int kStateSize = 128 + 8;
  void SaveState(uint8_t* out) const;
  void LoadState(const uint8_t* in);

  // Local time the emulated clock shows now, in seconds since 1970.
  int64_t Now() const;

 private:
  void Latch();              // Time registers from Now().
  void CatchUp();            // Raise UF/AF for seconds that passed.
  uint8_t Encode(int value) const;
  int Decode(uint8_t value) const;

  Clock clock_;
  uint8_t regs_[128] = {};
  int64_t offset_ = 0;       // Emulated minus host seconds.
  int64_t last_checked_ = 0; // Last second evaluated for the flags.
};

#endif
