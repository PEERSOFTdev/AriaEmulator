#include "aria_machine.h"

#include <algorithm>
#include <cstring>

#include "hle_bios.h"

namespace {

constexpr uint32_t kWindowBase = 0xD0000;  // Segment D000h, 64 KB (HANDOFF 6.2).
constexpr uint32_t kWindowSize = 0x10000;

}  // namespace

AriaMachine::AriaMachine(const FlashImage& flash, std::vector<uint8_t> kernel, Rtc146818::Clock clock)
    : ram_(1 << 20, 0),
      flash_(flash.bytes),
      kernel_(std::move(kernel)),
      rtc_(std::move(clock)),
      cpu_(*this),
      bios_(std::make_unique<HleBios>(*this)),
      dsp_(*this) {
  disks_[0] = MakeRomDisk();
  disks_[1] = MakeFlashDisk(flash);
  cpu_.SetTrapHandler([this](uint8_t number) { bios_->Trap(number); });
}

AriaMachine::~AriaMachine() = default;

void AriaMachine::PowerOn() {
  std::fill(ram_.begin(), ram_.end(), 0);
  latch_580_ = latch_480_ = 0;
  powered_off_ = false;
  ucs_start_ = 0xFFC00;
  dsp_bytes_.clear();
  samples_.clear();
  sample_dividers_.clear();
  dsp_log_.clear();
  dsp_control_ = 0;
  dsp_control_writes_ = 0;
  dsp_state_ = DspState::kReset;
  dsp_boots_ = 0;
  mailbox_full_ = false;
  dsp_debt_ = 0;
  dsp_.Reset();
  cpu_.Reset();
  bios_->PowerOn(kernel_);
  UpdateHostLines();
}

void AriaMachine::Run(int cycles) {
  if (powered_off_) return;
  // The CPU and the DSP in short alternating slices: a byte through the
  // host mailbox needs the DSP to read it before the CPU's DMA sends the
  // next one, so long slices would throttle SPEAK's stream.
  constexpr int kSlice = 64;
  for (int done = 0; done < cycles && !powered_off_;) {
    const int used = cpu_.Run(std::min(kSlice, cycles - done));
    done += used;
    if (dsp_state_ != DspState::kRunning) continue;
    dsp_debt_ += static_cast<int64_t>(used) * kDspHz;
    const int dsp_cycles = static_cast<int>(dsp_debt_ / kCpuHz);
    if (dsp_cycles > 0) {
      dsp_debt_ -= static_cast<int64_t>(dsp_.Run(dsp_cycles)) * kCpuHz;
    }
  }
}

// ---- The DSP's host interface (HANDOFF 6.3) ----

void AriaMachine::UpdateHostLines() {
  // An empty mailbox says "next byte" to the CPU in two ways: DRQ0 paces
  // SPEAK's DMA channel 0 into 0402h, both the boot image (SPEAK.EXE+0A1Eh,
  // control 76A6h) and the speech frames (+0F61h, 77A6h); INT3 is the line
  // SPEAK polls in the request register before each command byte (+0A7Bh,
  // bit 7 of 2Eh, with INT3 masked and level-triggered by writing 1Fh to
  // 3Eh, +6792h). Whether the board has one signal or two is unknown.
  const bool ready = !mailbox_full_;
  cpu_.SetDmaRequest(0, ready);
  cpu_.SetInterruptPin(3, ready);
}

void AriaMachine::WriteDspControl(uint8_t value) {
  // SPEAK writes (shadow XOR 7) OR 8 from its copy at DS:561Ch
  // (SPEAK.EXE+896Ah), so bit 3 is set in every write except the plain 0
  // before booting (+09F9h) and the 8 after it (+0A14h): bit 3 releases
  // the DSP's reset. The meaning of bits 0-2 is unknown.
  const bool was_running = (dsp_control_ & 0x08) != 0;
  dsp_control_ = value;
  ++dsp_control_writes_;
  const bool run = (value & 0x08) != 0;
  if (!run) {
    dsp_state_ = DspState::kReset;
  } else if (!was_running) {
    // Out of reset with MMAP = 0: the DSP boots page 0 from its boot
    // memory, which on the Aria is the host mailbox (HANDOFF 6.3).
    dsp_.Reset();
    dsp_.BeginBoot();
    dsp_state_ = DspState::kBooting;
    ++dsp_boots_;
    if (mailbox_full_) WriteDspData(mailbox_);
  }
}

void AriaMachine::WriteDspData(uint8_t value) {
  mailbox_ = value;
  mailbox_full_ = true;
  if (dsp_state_ == DspState::kBooting) {
    // The boot sequencer reads each byte as soon as it is there.
    mailbox_full_ = false;
    if (dsp_.BootByte(value)) dsp_state_ = DspState::kRunning;
  } else if (dsp_state_ == DspState::kRunning) {
    // IRQ2, set to edge-triggered by LPC.DSP (ICNTL = 14h at 0042h).
    dsp_.SetInterrupt(Adsp2105::kIrq2, true);
  }
  UpdateHostLines();
}

uint16_t AriaMachine::ReadExternalData(uint32_t address) {
  // LPC.DSP and PLAY.DSP read the mailbox at DM(3000h) and nothing else
  // outside the chip (LPC.DSP 005Fh, 0082h; PLAY.DSP 00C9h).
  if (address != 0x3000) {
    ++unknown_ports_[static_cast<uint16_t>(0xD000 | (address & 0x0FFF))];
    return 0xFFFF;
  }
  mailbox_full_ = false;
  dsp_.SetInterrupt(Adsp2105::kIrq2, false);
  UpdateHostLines();
  return mailbox_;
}

// ---- Memory ----

bool AriaMachine::WindowMapped(uint32_t address) const {
  // 0580h bits 1-0: 0 off, 2 internal flash, 3 card (HANDOFF 6.2). Mode 1
  // is probably RAM (ARIACONS.PAS ENABLE_RAM); nothing seen uses it yet, so
  // it falls through to RAM, which is what "RAM in the window" would show.
  const uint8_t mode = latch_580_ & 0x03;
  return (mode == 2 || mode == 3) && address >= kWindowBase && address < kWindowBase + kWindowSize;
}

uint8_t AriaMachine::ReadWindow(uint32_t address) const {
  if ((latch_580_ & 0x03) == 3) return 0xFF;  // Card: none inserted yet (HANDOFF 6.8).
  // Flash page = 0580h bits 7-2; flash address = offset in the B: image.
  const uint32_t page = latch_580_ >> 2;
  const std::size_t flat = static_cast<std::size_t>(page) * kWindowSize + (address - kWindowBase);
  return flat < flash_.size() ? flash_[flat] : 0xFF;
}

uint8_t AriaMachine::ReadMemory(uint32_t address) {
  if (WindowMapped(address)) return ReadWindow(address);
  if (address >= ucs_start_) {
    // Upper chip select: the boot ROM. Only the top 1 KB is ours; below
    // it, inside a larger UCS, the original ROM disk A: would have been.
    return address >= kRomStub ? rom_stub_[address - kRomStub] : 0xFF;
  }
  return ram_[address];
}

void AriaMachine::WriteMemory(uint32_t address, uint8_t value) {
  if (WindowMapped(address)) return;  // Flash programming is not emulated (HANDOFF 6.11).
  if (address >= ucs_start_) return;  // ROM.
  ram_[address] = value;
}

void AriaMachine::ChipSelect(int which, uint16_t value) {
  // UMCS: bits 15-6 give the start address in 1 KB units (80186 manual);
  // RAMDISK.EXE writes FFF8h to shrink it to FFC00h (HANDOFF 6.2).
  if (which == 0) ucs_start_ = static_cast<uint32_t>(value & 0xFFC0) << 4;
}

// ---- I/O ----

uint8_t AriaMachine::ReadIo(uint16_t port) {
  if (port >= 0x0500 && port <= 0x057F) return rtc_.Read(static_cast<uint8_t>(port - 0x0500));
  switch (port) {
    case 0x0480:
      // Card slot status: bits 0-1 card detect, active low; both high = no
      // card (HANDOFF 6.8). Bits 2-3 (BVD) high = battery good.
      return 0x0F;
    case 0x0581: return key_rows_[0];
    case 0x0582: return key_rows_[1];
    case 0x0584: return key_rows_[2];
    case 0x0700: return battery_;
    default:
      ++unknown_ports_[port];
      return 0xFF;
  }
}

void AriaMachine::WriteIo(uint16_t port, uint8_t value) {
  if (port >= 0x0500 && port <= 0x057F) {
    rtc_.Write(static_cast<uint8_t>(port - 0x0500), value);
    return;
  }
  switch (port) {
    case 0x0480: latch_480_ = value; return;
    case 0x0580: latch_580_ = value; return;
    case 0x0401: WriteDspControl(value); return;
    case 0x0402:
      // DSP data port: boot image, command bytes and speech frames.
      dsp_bytes_.push_back(value);
      WriteDspData(value);
      return;
    case 0x0701:
      // TURNOFF.COM writes 0 here and halts (HANDOFF 6.9).
      if (value == 0) powered_off_ = true;
      return;
    case 0x0702: slot_power_ = value; return;
    default:
      ++unknown_ports_[port];
      return;
  }
}

// ---- Disks ----

BiosDisk AriaMachine::MakeRomDisk() const {
  // A small empty FAT12 volume standing in for the ROM disk A: of the boot
  // ROM, whose contents are lost (HANDOFF 2). FreeDOS needs two floppy-type
  // drives, or it makes B: a phantom of A:.
  BiosDisk disk;
  disk.sectors_per_track = 8;
  disk.heads = 2;
  disk.sectors = 128;  // 64 KB.
  disk.data.assign(disk.sectors * 512, 0);
  uint8_t* boot = disk.data.data();
  const uint8_t header[] = {0xEB, 0x3C, 0x90, 'A', 'R', 'I', 'A', 'R', 'O', 'M', ' ',
                            0x00, 0x02,   // 512 bytes per sector
                            0x01,         // 1 sector per cluster
                            0x01, 0x00,   // 1 reserved
                            0x02,         // 2 FATs
                            0x10, 0x00,   // 16 root entries
                            0x80, 0x00,   // 128 sectors
                            0xF8,         // media
                            0x01, 0x00,   // 1 sector per FAT
                            0x08, 0x00,   // sectors per track
                            0x02, 0x00};  // heads
  std::memcpy(boot, header, sizeof header);
  boot[510] = 0x55;
  boot[511] = 0xAA;
  for (int fat = 0; fat < 2; ++fat) {
    uint8_t* table = disk.data.data() + (1 + fat) * 512;
    table[0] = 0xF8;
    table[1] = table[2] = 0xFF;
  }
  uint8_t* root = disk.data.data() + 3 * 512;
  std::memcpy(root, "ARIA ROM   ", 11);
  root[11] = 0x08;  // Volume label.
  return disk;
}

BiosDisk AriaMachine::MakeFlashDisk(const FlashImage& flash) const {
  BiosDisk disk;
  disk.data = flash.bytes;
  const uint16_t total = static_cast<uint16_t>(flash.bytes[19] | flash.bytes[20] << 8);
  disk.sectors = total ? total : static_cast<uint32_t>(flash.bytes.size() / 512);
  disk.sectors_per_track = 63;
  disk.heads = 16;
  disk.patch_boot_record = true;
  return disk;
}
