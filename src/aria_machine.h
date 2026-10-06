#ifndef ARIA_ARIA_MACHINE_H
#define ARIA_ARIA_MACHINE_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cpu80188.h"
#include "flash_image.h"
#include "rtc146818.h"

class HleBios;

// A drive the emulator's BIOS serves through INT 13h (HANDOFF 6.11).
struct BiosDisk {
  std::vector<uint8_t> data;  // Image; writes change this copy only.
  uint16_t sectors_per_track = 0;
  uint16_t heads = 0;
  uint32_t sectors = 0;
  // Geometry the BIOS reports and uses for CHS; for drive B: it differs
  // from the boot record, which has 61440 sectors per track and would not
  // fit the 6-bit sector field (HANDOFF 6.11). The boot record returned
  // through INT 13h is patched to match.
  bool patch_boot_geometry = false;
};

// The Aria board around the 80C188 (HANDOFF 2, 6.2-6.9, hardware-map.md):
// memory decoding with the D000h window, the latches, the RTC, the key
// matrix and battery status, and the emulator's BIOS in place of the
// missing boot ROM (HANDOFF 6.1, 6.10).
class AriaMachine : public CpuBus {
 public:
  static constexpr uint32_t kRomStub = 0xFFC00;  // Top 1 KB, ROM under any UMCS.
  static constexpr int kCpuHz = 8'000'000;       // Assumption: real clock unknown (HANDOFF 6.12).

  AriaMachine(const FlashImage& flash, std::vector<uint8_t> kernel, Rtc146818::Clock clock = {});
  ~AriaMachine() override;

  // Power-on: BIOS set-up, then the FreeDOS kernel started from drive B:.
  void PowerOn();
  // Runs about `cycles` CPU cycles.
  void Run(int cycles);

  Cpu80188& cpu() { return cpu_; }
  HleBios& bios() { return *bios_; }
  Rtc146818& rtc() { return rtc_; }
  BiosDisk& disk(int drive) { return disks_[drive & 1]; }

  // Direct memory access for the BIOS and tests, through the same decoding
  // as the CPU sees.
  uint8_t Peek(uint32_t address) { return ReadMemory(address & 0xFFFFF); }
  void Poke(uint32_t address, uint8_t value) { WriteMemory(address & 0xFFFFF, value); }
  uint16_t PeekWord(uint32_t address) {
    return static_cast<uint16_t>(Peek(address) | Peek(address + 1) << 8);
  }
  void PokeWord(uint32_t address, uint16_t value) {
    Poke(address, static_cast<uint8_t>(value));
    Poke(address + 1, static_cast<uint8_t>(value >> 8));
  }
  uint8_t* RomStub() { return rom_stub_; }

  // Key matrix rows (0581h, 0582h, 0584h) and battery status (0700h).
  void SetKeyRow(int row, uint8_t bits) { key_rows_[row % 3] = bits; }
  void SetBatteryStatus(uint8_t value) { battery_ = value; }
  bool PoweredOff() const { return powered_off_; }

  // Ports the model does not implement, with access counts.
  const std::map<uint16_t, int>& UnknownPorts() const { return unknown_ports_; }

  // CpuBus.
  uint8_t ReadMemory(uint32_t address) override;
  void WriteMemory(uint32_t address, uint8_t value) override;
  uint8_t ReadIo(uint16_t port) override;
  void WriteIo(uint16_t port, uint8_t value) override;
  void ChipSelect(int which, uint16_t value) override;

 private:
  bool WindowMapped(uint32_t address) const;
  uint8_t ReadWindow(uint32_t address) const;
  BiosDisk MakeRomDisk() const;
  BiosDisk MakeFlashDisk(const FlashImage& flash) const;

  std::vector<uint8_t> ram_;
  uint8_t rom_stub_[1024] = {};
  std::vector<uint8_t> flash_;    // Flash contents as the window sees them.
  std::vector<uint8_t> kernel_;
  BiosDisk disks_[2];             // 0 = A: ROM disk, 1 = B: flash.
  Rtc146818 rtc_;
  Cpu80188 cpu_;
  std::unique_ptr<HleBios> bios_;

  uint32_t ucs_start_ = 0xFFC00;  // Upper chip select start (UMCS).
  uint8_t latch_580_ = 0;         // Window mode and page (HANDOFF 6.2).
  uint8_t latch_480_ = 0;         // High page bits for the card.
  uint8_t key_rows_[3] = {};
  uint8_t battery_ = 0x2F;        // Adapter connected, fully charged (HANDOFF 6.4).
  uint8_t slot_power_ = 0;
  bool powered_off_ = false;
  std::map<uint16_t, int> unknown_ports_;
};

#endif
