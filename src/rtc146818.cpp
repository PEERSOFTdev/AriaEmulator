#include "rtc146818.h"

#include <chrono>
#include <cstring>
#include <ctime>

namespace {

constexpr uint8_t kRegA = 0x0A, kRegB = 0x0B, kRegC = 0x0C, kRegD = 0x0D;
constexpr uint8_t kBSet = 0x80, kBBinary = 0x04, kB24Hour = 0x02;
constexpr uint8_t kCAlarm = 0x20, kCUpdate = 0x10, kCIrq = 0x80;
constexpr uint8_t kDValid = 0x80;

int64_t HostLocalSeconds() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  // Seconds since 1970 as if local time were UTC: what a PC clock shows.
  int64_t days = 0;
  for (int year = 1970; year < local.tm_year + 1900; ++year) {
    days += (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
  }
  days += local.tm_yday;
  return days * 86400 + local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
}

// Civil date from days since 1970 (Howard Hinnant's algorithm).
void CivilFromDays(int64_t days, int& year, int& month, int& day) {
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const int64_t doe = days - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(yoe + era * 400 + (month <= 2 ? 1 : 0));
}

int64_t DaysFromCivil(int year, int month, int day) {
  year -= month <= 2 ? 1 : 0;
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const int64_t yoe = year - era * 400;
  const int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

}  // namespace

Rtc146818::Rtc146818(Clock clock) : clock_(clock ? std::move(clock) : Clock(HostLocalSeconds)) {
  regs_[kRegA] = 0x26;               // 32.768 kHz time base, 1024 Hz periodic rate.
  regs_[kRegB] = kB24Hour;           // BCD, 24-hour, no interrupts.
  regs_[kRegD] = kDValid;            // Battery good.
  // CMOS defaults of the settings ARIAMODE keeps there (HANDOFF 6.5, 6.14):
  // "R" = the newer card support on, so TRICKSS2 installs; no "C", so the
  // driver for an external IBM keyboard (ATKBD) leaves.
  regs_[0x7F] = 'R';
  last_checked_ = Now();
}

int64_t Rtc146818::Now() const { return clock_() + offset_; }

uint8_t Rtc146818::Encode(int value) const {
  if (regs_[kRegB] & kBBinary) return static_cast<uint8_t>(value);
  return static_cast<uint8_t>((value / 10) << 4 | value % 10);
}

int Rtc146818::Decode(uint8_t value) const {
  if (regs_[kRegB] & kBBinary) return value;
  return (value >> 4) * 10 + (value & 0x0F);
}

void Rtc146818::Latch() {
  const int64_t now = Now();
  const int64_t days = now >= 0 ? now / 86400 : (now - 86399) / 86400;
  const int seconds_of_day = static_cast<int>(now - days * 86400);
  int year = 0, month = 0, day = 0;
  CivilFromDays(days, year, month, day);
  const int hour = seconds_of_day / 3600;
  regs_[0x00] = Encode(seconds_of_day % 60);
  regs_[0x02] = Encode(seconds_of_day / 60 % 60);
  if (regs_[kRegB] & kB24Hour) {
    regs_[0x04] = Encode(hour);
  } else {
    const int twelve = hour % 12 == 0 ? 12 : hour % 12;
    regs_[0x04] = static_cast<uint8_t>(Encode(twelve) | (hour >= 12 ? 0x80 : 0));
  }
  regs_[0x06] = static_cast<uint8_t>((days + 4) % 7 + 1);  // 1 = Sunday; 1.1.1970 was a Thursday.
  regs_[0x07] = Encode(day);
  regs_[0x08] = Encode(month);
  regs_[0x09] = Encode(year % 100);
}

void Rtc146818::CatchUp() {
  const int64_t now = Now();
  if (now <= last_checked_) return;
  // An alarm byte of C0h-FFh means "don't care" (HANDOFF 6.5).
  auto matches = [&](uint8_t alarm, int value) {
    return (alarm & 0xC0) == 0xC0 || Decode(alarm) == value;
  };
  const int64_t from = now - last_checked_ > 86400 ? now - 86400 : last_checked_ + 1;
  for (int64_t second = from; second <= now; ++second) {
    const int64_t days = second >= 0 ? second / 86400 : (second - 86399) / 86400;
    const int of_day = static_cast<int>(second - days * 86400);
    regs_[kRegC] |= kCUpdate;
    if (matches(regs_[0x01], of_day % 60) && matches(regs_[0x03], of_day / 60 % 60) &&
        matches(regs_[0x05], of_day / 3600)) {
      regs_[kRegC] |= kCAlarm;
    }
  }
  last_checked_ = now;
}

uint8_t Rtc146818::Read(uint8_t reg) {
  reg &= 0x7F;
  if (reg <= 0x09 && reg != 0x01 && reg != 0x03 && reg != 0x05) {
    if (!(regs_[kRegB] & kBSet)) Latch();
    return regs_[reg];
  }
  if (reg == kRegA) return regs_[kRegA] & 0x7F;  // Never "update in progress".
  if (reg == kRegC) {
    CatchUp();
    uint8_t value = regs_[kRegC];
    const uint8_t enabled = regs_[kRegB] & 0x70;  // PIE AIE UIE line up with PF AF UF.
    if (value & enabled) value |= kCIrq;
    regs_[kRegC] = 0;  // Reading register C clears the flags.
    return value;
  }
  return regs_[reg];
}

void Rtc146818::Write(uint8_t reg, uint8_t value) {
  reg &= 0x7F;
  if (reg == kRegC || reg == kRegD) return;  // Read-only.
  if (reg <= 0x09 && reg != 0x01 && reg != 0x03 && reg != 0x05) {
    // Setting the time: take the current values, change one, and move the
    // offset so that the clock shows the result.
    if (!(regs_[kRegB] & kBSet)) Latch();
    regs_[reg] = value;
    int hour = Decode(static_cast<uint8_t>(regs_[0x04] & 0x7F));
    if (!(regs_[kRegB] & kB24Hour)) {
      hour %= 12;
      if (regs_[0x04] & 0x80) hour += 12;
    }
    const int year2 = Decode(regs_[0x09]);
    const int year = year2 < 80 ? 2000 + year2 : 1900 + year2;
    const int64_t days = DaysFromCivil(year, Decode(regs_[0x08]), Decode(regs_[0x07]));
    const int64_t wanted = days * 86400 + hour * 3600 + Decode(regs_[0x02]) * 60 + Decode(regs_[0x00]);
    offset_ = wanted - clock_();
    last_checked_ = wanted;
    return;
  }
  if (reg == kRegB) {
    const bool leaving_set = (regs_[kRegB] & kBSet) && !(value & kBSet);
    regs_[kRegB] = value;
    if (leaving_set) last_checked_ = Now();
    return;
  }
  regs_[reg] = value;
}

void Rtc146818::LoadCmos(const uint8_t* bytes) {
  std::memcpy(regs_ + 0x0E, bytes + 0x0E, sizeof regs_ - 0x0E);
}
