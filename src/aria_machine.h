#ifndef ARIA_ARIA_MACHINE_H
#define ARIA_ARIA_MACHINE_H

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "adsp2105.h"
#include "cpu80188.h"
#include "dac_renderer.h"
#include "flash_image.h"
#include "pc_card.h"
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
  // through INT 13h is patched to match, and gets the 55AAh signature
  // that DLRDISK never wrote.
  bool patch_boot_record = false;
};

// The Aria board around the 80C188 (HANDOFF 2, 6.2-6.9, hardware-map.md):
// memory decoding with the D000h window, the latches, the RTC, the key
// matrix and battery status, and the emulator's BIOS in place of the
// missing boot ROM (HANDOFF 6.1, 6.10).
class AriaMachine : public CpuBus, private Adsp2105Board {
 public:
  static constexpr uint32_t kRomStub = 0xFFC00;  // Top 1 KB, ROM under any UMCS.
  // The rest of the emulator's "boot ROM", F0000h-FFBFFh, under the upper
  // chip select as the BIOS sets it: on the Aria the boot ROM, with ROM-DOS
  // running from it, covered the top of the RAM disk's memory, which
  // RAMDISK.EXE reaches only by shrinking UMCS to FFF8h with interrupts off
  // (RAMDISK.EXE+0372h-+03A9h, HANDOFF 6.2). Here it holds the FreeDOS
  // kernel's resident code (HleBios::PatchKernel), so that it takes no DOS
  // memory, as ROM-DOS took none. Writable like shadow RAM: the kernel
  // copies itself in at start-up. Where the real ROM began is unknown.
  static constexpr uint32_t kRomArea = 0xF0000;
  // 15 MHz, the timers 3.75 MHz (HANDOFF 6.4): FOSSIL's baud divisors give
  // a 16x clock of 3.75 MHz (FOSSIL.COM+0793h), BKB counts 250 ms of key
  // delay as 18 scans of timer 2 at C925h (BKB.COM+1012h), and AAPI's 1 ms
  // delay waits a timer 0 period of 3750 (AAPI.EXE+10B35).
  static constexpr int kCpuHz = 15'000'000;
  // The PC's tick, 1193182 / 65536 Hz: the BIOS counts 1800B0h ticks a day
  // for INT 1Ah, which DOS's clock relies on. No 16-bit timer makes it at
  // 3.75 MHz, so the board has some other source, unknown (HANDOFF 6.1).
  static constexpr int64_t kTickNumerator = 1'193'182;
  static constexpr int64_t kTickDenominator = 65'536;
  // Assumption: the DSP's instruction rate. The Aria's crystal is unknown;
  // 10.24 MHz is a 2105 speed grade's range and, with a frame of 32 serial
  // clocks, gives Czech speech the 32 kHz listening chose (HANDOFF 6.3).
  static constexpr int kDspHz = 10'240'000;

  AriaMachine(const FlashImage& flash, std::vector<uint8_t> kernel, Rtc146818::Clock clock = {});
  ~AriaMachine() override;

  // Power-on: BIOS set-up, then the FreeDOS kernel started from drive B:.
  // The battery keeps the RAM, the RAM disk D: in it, and the clock's CMOS
  // through power-off and reset (HANDOFF 6.9), so they stay as they were.
  void PowerOn();
  // The battery disconnected: RAM and CMOS are lost, the clock falls back
  // to host time. A new machine starts this way.
  void BatteryCut();
  // Switched off from outside, as by the Aria's own write of 0 to 0701h
  // (TURNOFF.COM): the CPU stops, the battery keeps RAM and clock.
  void PowerOff() { powered_off_ = true; }

  // What the battery keeps, for the host to save between sessions: the whole
  // RAM, then the clock chip (Rtc146818::SaveState).
  static constexpr std::size_t kStateSize = (1u << 20) + Rtc146818::kStateSize;
  void SaveState(std::vector<uint8_t>& out) const;
  // False, with the machine untouched, when `in` is not kStateSize long.
  bool LoadState(const std::vector<uint8_t>& in);
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

  // The PCMCIA slot, drive C: (HANDOFF 6.8). The drivers poll the card
  // detect bits of 0480h from INT 1Ch, so a card can come and go while the
  // machine runs, as on the Aria. DLFFSA takes a media change only from an
  // empty slot seen on a tick: swapped within 30 ms DOS went on with the
  // old card's directory, from 60 ms on it never did (boot_test, 10. 10.
  // 2026). A card inserted before the empty slot has lived through
  // kCardSwapTicks ticks (about 0.27 s) therefore waits, out of the slot,
  // until it has; at power-on it goes straight in. Ticks, not time: only a
  // tick runs the drivers' INT 1Ch.
  static constexpr int kCardSwapTicks = 5;
  // The slot must be empty (EjectCard first).
  void InsertCard(PcCard card);
  // The card in the slot, or the one still waiting to go in.
  std::optional<PcCard> EjectCard();
  PcCard* card() { return card_ ? &*card_ : nullptr; }
  bool CardWaiting() const { return pending_card_.has_value(); }
  uint8_t SlotPower() const { return slot_power_; }
  bool PoweredOff() const { return powered_off_; }

  // The DSP side (HANDOFF 6.3): every byte SPEAK sends to the data port
  // 0402h, by DMA or by hand, the control latch 0401h, and the samples the
  // DSP shifts out of SPORT1.
  const std::vector<uint8_t>& DspBytes() const { return dsp_bytes_; }
  int DspControlWrites() const { return dsp_control_writes_; }
  uint8_t DspControl() const { return dsp_control_; }
  // The sound the machine made since the last call, at kAudioHz.
  std::vector<int16_t> TakeAudio() { return dac_.Take(); }
  static constexpr int kAudioHz = 44100;
  // Keep every SPORT1 word for tests and WAV dumps (off in the application,
  // where the stream would grow without end).
  void KeepSamples(bool keep) { keep_samples_ = keep; }
  const std::vector<int16_t>& Samples() const { return samples_; }
  // SPORT1's SCLKDIV for each sample: 4 for LPC.DSP speech, 6-19 for
  // PLAY.DSP sounds; the sample's duration is proportional to it + 1.
  const std::vector<uint16_t>& SampleDividers() const { return sample_dividers_; }
  Adsp2105& dsp() { return dsp_; }
  // The DSP's diagnostic messages (first 200), with its program counter.
  const std::vector<std::string>& DspLog() const { return dsp_log_; }
  int DspBoots() const { return dsp_boots_; }

  // Ports the model does not implement, with access counts.
  const std::map<uint16_t, int>& UnknownPorts() const { return unknown_ports_; }

  // CpuBus.
  uint8_t ReadMemory(uint32_t address) override;
  void WriteMemory(uint32_t address, uint8_t value) override;
  uint8_t ReadIo(uint16_t port) override;
  void WriteIo(uint16_t port, uint8_t value) override;
  void ChipSelect(int which, uint16_t value) override;
  // The tick (kTickNumerator) sets the board's tick latch, which drives
  // INT2; the BIOS's INT 0Eh handler clears it (HANDOFF 6.1, hle_bios.cpp).
  void AcknowledgeTick();

 private:
  // Adsp2105Board.
  void SportOutput(uint16_t sample, uint16_t clock_divider) override {
    if (keep_samples_) {
      samples_.push_back(static_cast<int16_t>(sample));
      sample_dividers_.push_back(clock_divider);
    }
    dac_.Sample(slice_base_ + (dsp_.Cycles() - slice_dsp_start_), static_cast<int16_t>(sample));
  }
  uint16_t ReadExternalData(uint32_t address) override;
  void Log(const char* text) override {
    if (dsp_log_.size() < 200) dsp_log_.emplace_back(text);
  }

  void WriteDspControl(uint8_t value);
  void WriteDspData(uint8_t value);
  void UpdateHostLines();

  bool WindowMapped(uint32_t address) const;
  uint8_t ReadWindow(uint32_t address) const;
  uint32_t CardAddress(uint32_t address) const;
  BiosDisk MakeRomDisk() const;
  BiosDisk MakeFlashDisk(const FlashImage& flash) const;

  std::vector<uint8_t> ram_;
  uint8_t rom_stub_[1024] = {};
  std::vector<uint8_t> rom_area_;  // kRomArea up to kRomStub.
  std::vector<uint8_t> flash_;    // Flash contents as the window sees them.
  std::vector<uint8_t> kernel_;
  BiosDisk disks_[2];             // 0 = A: ROM disk, 1 = B: flash.
  Rtc146818::Clock clock_;        // For a new RTC after a battery cut.
  Rtc146818 rtc_;
  Cpu80188 cpu_;
  std::unique_ptr<HleBios> bios_;

  uint32_t ucs_start_ = 0xFFC00;  // Upper chip select start (UMCS).
  uint8_t latch_580_ = 0;         // Window mode and page (HANDOFF 6.2).
  uint8_t latch_480_ = 0;         // High page bits for the card.
  uint8_t key_rows_[3] = {};
  uint8_t battery_ = 0x2F;        // Adapter connected, fully charged (HANDOFF 6.4).
  uint8_t slot_power_ = 0;
  std::optional<PcCard> card_;
  std::optional<PcCard> pending_card_;  // Inserted, waiting for kCardSwapTicks.
  int empty_slot_ticks_ = kCardSwapTicks;  // Ticks since the last eject; a new slot has been empty long.
  bool powered_off_ = false;
  Adsp2105 dsp_;
  enum class DspState { kReset, kBooting, kRunning };
  DspState dsp_state_ = DspState::kReset;
  int dsp_boots_ = 0;
  bool mailbox_full_ = false;     // The one-byte host latch (HANDOFF 6.3).
  uint8_t mailbox_ = 0;
  int64_t dsp_debt_ = 0;          // DSP cycles owed, in CPU-Hz units.
  int64_t tick_debt_ = 0;         // Toward the next tick, in kTickNumerator units.
  std::vector<uint8_t> dsp_bytes_;
  std::vector<int16_t> samples_;
  std::vector<uint16_t> sample_dividers_;
  bool keep_samples_ = true;
  // The sound's timeline in DSP cycles; it runs on while the DSP is held in
  // reset. A slice's samples are placed from where it started.
  DacRenderer dac_{static_cast<double>(kDspHz), kAudioHz, 3000};
  int64_t audio_time_ = 0;
  int64_t slice_base_ = 0;
  int64_t slice_dsp_start_ = 0;
  std::vector<std::string> dsp_log_;
  uint8_t dsp_control_ = 0;
  int dsp_control_writes_ = 0;
  std::map<uint16_t, int> unknown_ports_;
};

#endif
