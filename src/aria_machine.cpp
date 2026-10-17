#include "aria_machine.h"

#include <algorithm>

#include "hle_bios.h"

namespace {

constexpr uint32_t kWindowBase = 0xD0000;  // Segment D000h, 64 KB (HANDOFF 6.2).
constexpr uint32_t kWindowSize = 0x10000;

}  // namespace

AriaMachine::AriaMachine(const FlashImage& flash, std::vector<uint8_t> kernel, Rtc146818::Clock clock)
    : ram_(1 << 20, 0),
      rom_area_(kRomStub - kRomArea, 0xFF),
      flash_(flash.bytes),
      kernel_(std::move(kernel)),
      clock_(std::move(clock)),
      rtc_(clock_),
      cpu_(*this),
      // The 8251's baud clock is TMROUT1: timer 1 runs on the CPU clock
      // over 4, its period max count A, 0 meaning 65536 (FOSSIL.COM+07DCh
      // programs it, control C001h). Stopped while EN is clear.
      serial_(
          [this]() -> int64_t {
            if (!(cpu_.TimerControl(1) & 0x8000)) return 0;
            const uint16_t max = cpu_.TimerMaxA(1);
            return 4 * static_cast<int64_t>(max ? max : 65536);
          },
          // Both level-triggered (FOSSIL writes 16h/17h to 38h/3Ah) and
          // updated inside the I/O access, so that reading the data drops
          // RxRDY before the handler's EOI (FOSSIL.COM+0908h).
          [this](bool rx_ready, bool tx_ready) {
            cpu_.SetInterruptPin(0, rx_ready);
            cpu_.SetInterruptPin(1, tx_ready);
          }),
      bios_(std::make_unique<HleBios>(*this)),
      dsp_(*this) {
  disks_[0] = MakeRomDisk();
  disks_[1] = MakeFlashDisk(flash);
  cpu_.SetTrapHandler([this](uint8_t number) { bios_->Trap(number); });
}

AriaMachine::~AriaMachine() = default;

void AriaMachine::BatteryCut() {
  std::fill(ram_.begin(), ram_.end(), 0);
  rtc_ = Rtc146818(clock_);
}

void AriaMachine::SaveState(std::vector<uint8_t>& out) const {
  out.assign(ram_.begin(), ram_.end());
  out.resize(kStateSize);
  rtc_.SaveState(out.data() + ram_.size());
}

bool AriaMachine::LoadState(const std::vector<uint8_t>& in) {
  if (in.size() != kStateSize) return false;
  std::copy(in.begin(), in.begin() + static_cast<std::ptrdiff_t>(ram_.size()), ram_.begin());
  rtc_.LoadState(in.data() + ram_.size());
  return true;
}

void AriaMachine::PowerOn() {
  latch_580_ = latch_480_ = 0;
  slot_power_ = 0;  // DLFFSA powers a card it finds (CardPowered).
  powered_off_ = false;
  ucs_start_ = 0xFFC00;
  // ROM contents are the BIOS's to set up; the kernel copies itself in anew
  // at every start, and none of it is battery-kept state.
  std::fill(rom_area_.begin(), rom_area_.end(), 0xFF);
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
  tick_debt_ = 0;
  dac_ = DacRenderer(static_cast<double>(kDspHz), kAudioHz, 3000);
  audio_time_ = 0;
  dsp_.Reset();
  cpu_.Reset();
  serial_.Reset();
  keyboard_adapter_.Reset();
  bios_->PowerOn(kernel_);
  UpdateHostLines();
  // The drivers start afresh and look at the slot themselves.
  if (pending_card_) {
    card_ = std::move(pending_card_);
    pending_card_.reset();
  }
}

void AriaMachine::InsertCard(PcCard card) {
  if (empty_slot_ticks_ >= kCardSwapTicks) {
    card_ = std::move(card);
  } else {
    pending_card_ = std::move(card);
  }
}

std::optional<PcCard> AriaMachine::EjectCard() {
  const bool was_in = card_.has_value();
  std::optional<PcCard> out = was_in ? std::move(card_) : std::move(pending_card_);
  if (was_in) empty_slot_ticks_ = 0;
  card_.reset();
  pending_card_.reset();
  return out;
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
    serial_.Advance(used);
    // The tick sets a latch that holds INT2 until the BIOS takes it,
    // because MAME drops an external request as soon as its line falls.
    tick_debt_ += static_cast<int64_t>(used) * kTickNumerator;
    if (tick_debt_ >= static_cast<int64_t>(kCpuHz) * kTickDenominator) {
      tick_debt_ -= static_cast<int64_t>(kCpuHz) * kTickDenominator;
      cpu_.SetInterruptPin(2, true);
      if (!card_ && empty_slot_ticks_ < kCardSwapTicks) ++empty_slot_ticks_;
      if (pending_card_ && empty_slot_ticks_ >= kCardSwapTicks) {
        card_ = std::move(pending_card_);
        pending_card_.reset();
      }
    }
    dsp_debt_ += static_cast<int64_t>(used) * kDspHz;
    const int dsp_cycles = static_cast<int>(dsp_debt_ / kCpuHz);
    if (dsp_cycles <= 0) continue;
    int ran = dsp_cycles;
    if (dsp_state_ == DspState::kRunning) {
      slice_base_ = audio_time_;
      slice_dsp_start_ = dsp_.Cycles();
      ran = dsp_.Run(dsp_cycles);
    }
    dsp_debt_ -= static_cast<int64_t>(ran) * kCpuHz;
    audio_time_ += ran;
  }
  dac_.AdvanceTo(audio_time_);
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

uint32_t AriaMachine::CardAddress(uint32_t address) const {
  // Mode 3 (FORMAT.EXE+1DA9h, INT 69h mode 3 in VOICE.SYS, HANDOFF 6.2,
  // 6.8): card address bits 16-19 in 0580h bits 2-5, bits 20-25 in 0480h
  // bits 0-5.
  return static_cast<uint32_t>(latch_480_ & 0x3F) << 20 | static_cast<uint32_t>((latch_580_ >> 2) & 0x0F) << 16 |
         (address - kWindowBase);
}

uint8_t AriaMachine::ReadWindow(uint32_t address) const {
  if ((latch_580_ & 0x03) == 3) {
    // 0480h bit 7 is REG#: 0 selects attribute memory (FORMAT.EXE+1DA9h).
    // An empty or unpowered slot reads as a floating bus.
    return CardPowered() ? card_->Read(CardAddress(address), !(latch_480_ & 0x80)) : 0xFF;
  }
  // Flash page = 0580h bits 7-2; flash address = offset in the B: image.
  const uint32_t page = latch_580_ >> 2;
  const std::size_t flat = static_cast<std::size_t>(page) * kWindowSize + (address - kWindowBase);
  return flat < flash_.size() ? flash_[flat] : 0xFF;
}

uint8_t AriaMachine::ReadMemory(uint32_t address) {
  ++bus_bytes_;
  if (WindowMapped(address)) return ReadWindow(address);
  if (address >= ucs_start_) {
    // Upper chip select: the boot ROM. Ours is the top 1 KB and the area
    // from kRomArea; below that, inside a larger UCS, the original ROM
    // disk A: would have been.
    if (address >= kRomStub) return rom_stub_[address - kRomStub];
    return address >= kRomArea ? rom_area_[address - kRomArea] : 0xFF;
  }
  return ram_[address];
}

void AriaMachine::WriteMemory(uint32_t address, uint8_t value) {
  ++bus_bytes_;
  if (WindowMapped(address)) {
    // Card memory takes writes; the internal flash's programming is not
    // emulated (HANDOFF 6.11).
    // 0702h bit 0 is the slot's programming voltage (DLFFSA.EXE+0D58h).
    if ((latch_580_ & 0x03) == 3 && CardPowered())
      card_->Write(CardAddress(address), !(latch_480_ & 0x80), value, (slot_power_ & 0x01) != 0);
    return;
  }
  if (address >= ucs_start_) {
    if (address >= kRomArea && address < kRomStub) rom_area_[address - kRomArea] = value;
    return;
  }
  ram_[address] = value;
}

void AriaMachine::ChipSelect(int which, uint16_t value) {
  // UMCS: bits 15-6 give the start address in 1 KB units (80186 manual);
  // RAMDISK.EXE writes FFF8h to shrink it to FFC00h (HANDOFF 6.2).
  if (which == 0) ucs_start_ = static_cast<uint32_t>(value & 0xFFC0) << 4;
}

void AriaMachine::AcknowledgeTick() { cpu_.SetInterruptPin(2, false); }

// ---- I/O ----

uint8_t AriaMachine::ReadIo(uint16_t port) {
  ++bus_bytes_;
  if (port >= 0x0500 && port <= 0x057F) return rtc_.Read(static_cast<uint8_t>(port - 0x0500));
  if (port >= ParallelPort::kBase && port < ParallelPort::kBase + ParallelPort::kPorts)
    return parallel_.Read(port - ParallelPort::kBase);
  switch (port) {
    case 0x0480:
    {
      // Card slot status (HANDOFF 6.8): bits 0-1 card detect, active low,
      // both 0 = card in (FORMAT.EXE+1E00h); bits 2-3 BVD1/BVD2, both 1 =
      // battery good (BKB.COM+1622h); bit 5 write protect (FORMAT.EXE+06BAh).
      // A weak battery as BVD1 alone, a flat one as neither (PCMCIA): BKB's
      // battery report then says "částečně vybitá" and "zcela vybitá"
      // (boot_test, HANDOFF 6.8).
      if (!card_) return 0x0F;
      uint8_t status = card_->write_protected ? 0x20 : 0x00;
      if (card_->battery == PcCard::Battery::kGood) status |= 0x0C;
      if (card_->battery == PcCard::Battery::kLow) status |= 0x04;
      return status;
    }
    case 0x0600: return serial_.Read(0);
    case 0x0601: return serial_.Read(1);
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
  ++bus_bytes_;
  if (port >= 0x0500 && port <= 0x057F) {
    rtc_.Write(static_cast<uint8_t>(port - 0x0500), value);
    return;
  }
  if (port >= ParallelPort::kBase && port < ParallelPort::kBase + ParallelPort::kPorts) {
    parallel_.Write(port - ParallelPort::kBase, value);
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
    case 0x0600: serial_.Write(0, value); return;
    case 0x0601: serial_.Write(1, value); return;
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
  Fat12Format format;
  format.sectors = disk.sectors;
  format.root_entries = 16;
  format.sectors_per_track = disk.sectors_per_track;
  format.heads = disk.heads;
  format.oem = "ARIAROM ";
  format.label = "ARIA ROM";
  disk.data = FormatFat12(format);
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
