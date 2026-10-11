#include "fdc82077.h"

#include <cstdio>

namespace {

constexpr size_t kSectorSize = 512;
constexpr uint8_t kRate500 = 0, kRate250 = 2;  // CCR values.

// ST0 interrupt codes and the ST1/ST2 bits the driver tells apart
// (+2534h-+255Bh: 40h alone, ST1 02h write protect, ST1 10h overrun).
constexpr uint8_t kAbnormal = 0x40, kInvalid = 0x80, kSeekEnd = 0x20, kEquipmentCheck = 0x10;
constexpr uint8_t kMissingAddressMark = 0x01, kNotWritable = 0x02, kNoData = 0x04, kOverrun = 0x10,
                  kEndOfCylinder = 0x80;
constexpr uint8_t kWrongCylinder = 0x10;

// Command bytes by the low five bits of the first one (MT, MFM, SK above).
int CommandLength(uint8_t command) {
  switch (command & 0x1F) {
    case 0x03: return 3;  // SPECIFY
    case 0x04: return 2;  // SENSE DRIVE STATUS
    case 0x05: return 9;  // WRITE DATA
    case 0x06: return 9;  // READ DATA
    case 0x07: return 2;  // RECALIBRATE
    case 0x08: return 1;  // SENSE INTERRUPT
    case 0x0A: return 2;  // READ ID
    case 0x0D: return 6;  // FORMAT TRACK
    case 0x0F: return 3;  // SEEK
    case 0x10: return 1;  // VERSION
    case 0x12: return 2;  // PERPENDICULAR MODE
    case 0x13: return 4;  // CONFIGURE
    default: return 1;    // Invalid: answered with ST0 80h.
  }
}

}  // namespace

std::optional<Fdc82077::Disk> Fdc82077::Disk::FromImage(std::vector<uint8_t> bytes, bool write_protected) {
  Disk disk;
  if (bytes.size() == 80 * 2 * 9 * kSectorSize) {
    disk.sectors = 9;
    disk.rate = kRate250;
  } else if (bytes.size() == 80 * 2 * 18 * kSectorSize) {
    disk.sectors = 18;
    disk.rate = kRate500;
  } else {
    return std::nullopt;
  }
  disk.cylinders = 80;
  disk.heads = 2;
  disk.bytes = std::move(bytes);
  disk.write_protected = write_protected;
  return disk;
}

void Fdc82077::Insert(Disk disk) {
  disk_ = std::move(disk);
  changed_ = true;
}

std::optional<Fdc82077::Disk> Fdc82077::Eject() {
  std::optional<Disk> out = std::move(disk_);
  disk_.reset();
  changed_ = true;
  return out;
}

void Fdc82077::Reset() {
  command_.clear();
  result_.clear();
  pending_.clear();
  // In polling mode, the 82077's default, a reset leaves one status per
  // drive for SENSE INTERRUPT; the driver reads all four (+2587h).
  for (uint8_t d = 0; d < 4; ++d) pending_.push_back({static_cast<uint8_t>(0xC0 | d), cylinder_[d]});
  interrupt_ = true;
}

uint8_t Fdc82077::Read(int reg) {
  switch (reg) {
    case 2: return dor_;
    case 4:
      // MSR: RQM always (the execution phase is instant), DIO and busy in
      // the result phase, busy while taking command bytes.
      if (!result_.empty()) return 0xD0;
      return command_.empty() ? 0x80 : 0x90;
    case 5: {
      if (result_.empty()) return 0;
      interrupt_ = false;  // Reading the result clears the interrupt.
      const uint8_t value = result_.front();
      result_.pop_front();
      return value;
    }
    case 7:
      // DIR bit 7: the selected drive's disk-change line, not inverted for
      // this controller type ([8Eh] = 05h from +365Fh, tested at +2323h).
      return (dor_ & 3) == drive_ && changed_ ? 0x80 : 0x00;
    default:
      return 0xFF;
  }
}

void Fdc82077::Write(int reg, uint8_t value) {
  switch (reg) {
    case 2: {
      // DOR: bit 2 low holds the controller in reset; the driver writes
      // 08h, waits and writes 0Ch (+2561h).
      const bool released = !(dor_ & 4) && (value & 4);
      dor_ = value;
      if (!(value & 4)) {
        interrupt_ = false;
        command_.clear();
        result_.clear();
      } else if (released) {
        Reset();
      }
      break;
    }
    case 4:
      // DSR. The driver writes 80h (C0h at 1 Mbps) after the CCR (+2299h);
      // taken as an 82077 software reset it would queue four reset statuses
      // and the next seek would read drive 1's C1h as its own (+272Fh). So
      // only logged; the rate comes from the CCR.
      if (log) {
        char line[24];
        std::snprintf(line, sizeof line, "FDC DSR %02X", value);
        log(line);
      }
      break;
    case 5:
      if (!result_.empty()) break;
      command_.push_back(value);
      if (static_cast<int>(command_.size()) == CommandLength(command_[0])) {
        Execute();
        command_.clear();
      }
      break;
    case 7:
      rate_ = value & 3;  // CCR, from the table at +2AE2h (+225Ch).
      break;
    default:
      break;
  }
}

void Fdc82077::Result(std::initializer_list<uint8_t> bytes) {
  result_.assign(bytes.begin(), bytes.end());
}

Fdc82077::Disk* Fdc82077::Readable(int drive) {
  if (drive != drive_ || !disk_ || disk_->rate != rate_) return nullptr;
  return &*disk_;
}

void Fdc82077::Seek(int drive, int head, int cylinder) {
  const uint8_t unit = static_cast<uint8_t>(drive | head << 2);
  if (drive != drive_) {
    // No drive there: no track 0 signal, so an equipment check.
    pending_.push_back({static_cast<uint8_t>(kAbnormal | kSeekEnd | kEquipmentCheck | unit), cylinder_[drive]});
  } else {
    cylinder_[drive] = static_cast<uint8_t>(cylinder);
    // Stepping with a disk in clears the change line (+2331h-+2358h steps
    // to check it, and with no disk in reports 80h, not ready).
    if (disk_) changed_ = false;
    pending_.push_back({static_cast<uint8_t>(kSeekEnd | unit), cylinder_[drive]});
  }
  interrupt_ = true;
}

void Fdc82077::Execute() {
  const uint8_t command = command_[0];
  if (log) {
    std::string line = "FDC";
    char hex[4];
    for (uint8_t b : command_) {
      std::snprintf(hex, sizeof hex, " %02X", b);
      line += hex;
    }
    log(line);
  }
  const int drive = command_.size() > 1 ? command_[1] & 3 : 0;
  const int head = command_.size() > 1 ? (command_[1] >> 2) & 1 : 0;
  switch (command & 0x1F) {
    case 0x03:  // SPECIFY: step and head times, and ND = 0 (DMA mode).
    case 0x12:  // PERPENDICULAR MODE, sent because [8Eh] bit 0 (+2272h).
    case 0x13:  // CONFIGURE
      break;
    case 0x04: {  // SENSE DRIVE STATUS
      uint8_t st3 = static_cast<uint8_t>(drive | head << 2);
      if (drive == drive_) {
        st3 |= 0x28;  // Ready, two-sided.
        if (cylinder_[drive] == 0) st3 |= 0x10;  // Track 0; +2609h needs it after RECALIBRATE.
        if (disk_ && disk_->write_protected) st3 |= 0x40;
      }
      Result({st3});
      break;
    }
    case 0x05:
      ReadWrite(true);
      break;
    case 0x06:
      ReadWrite(false);
      break;
    case 0x07:  // RECALIBRATE
      Seek(drive, 0, 0);
      break;
    case 0x08:  // SENSE INTERRUPT
      interrupt_ = false;
      if (pending_.empty()) {
        Result({kInvalid});
      } else {
        Result({pending_.front()[0], pending_.front()[1]});
        pending_.pop_front();
      }
      break;
    case 0x0A:
      ReadId();
      break;
    case 0x0F:  // SEEK
      Seek(drive, head, command_[2]);
      break;
    case 0x10:  // VERSION: 90h is the 82077AA.
      Result({0x90});
      break;
    default:
      Result({kInvalid});
      break;
  }
}

void Fdc82077::ReadId() {
  const int drive = command_[1] & 3;
  const int head = (command_[1] >> 2) & 1;
  const uint8_t st0 = static_cast<uint8_t>(drive | head << 2);
  const uint8_t c = cylinder_[drive];
  Disk* disk = Readable(drive);
  if (!disk || c >= disk->cylinders) {
    Result({static_cast<uint8_t>(kAbnormal | st0), kMissingAddressMark, 0, c, static_cast<uint8_t>(head), 1, 2});
  } else {
    Result({st0, 0, 0, c, static_cast<uint8_t>(head), 1, 2});
  }
  interrupt_ = true;
}

// READ DATA and WRITE DATA, 9 bytes as the driver builds them (+237Ch):
// command, HD<<2|US, C, H, R, N, EOT, GPL, DTL. The sectors run from R to
// EOT (and on to head 1 with MT) until the chip's count ends; a normal
// end leaves R one past the last sector, which the driver uses to count
// what was done after an error (+1FDEh).
void Fdc82077::ReadWrite(bool write) {
  const bool multitrack = command_[0] & 0x80;
  const int drive = command_[1] & 3;
  int head = (command_[1] >> 2) & 1;
  uint8_t c = command_[2], h = command_[3], r = command_[4];
  const uint8_t n = command_[5], eot = command_[6];
  uint8_t st0 = 0, st1 = 0, st2 = 0;
  const int cylinder = cylinder_[drive];
  Disk* disk = Readable(drive);
  if (!disk || cylinder >= disk->cylinders) {
    st1 = kMissingAddressMark;
  } else if (n != 2) {
    st1 = kNoData;
  } else if (c != cylinder) {
    st1 = kNoData;
    st2 = kWrongCylinder;
  } else if (h != head) {
    // The sector IDs carry the head number. The driver starts with ID 0
    // for head 1 too, and on "no data" learns the real one by READ ID
    // ([0B6h + head], +203Ah-+2055h), so this must fail as on a real disk.
    st1 = kNoData;
  } else if (write && disk->write_protected) {
    st1 = kNotWritable;
  }
  while (!st1) {
    if (r < 1 || r > disk->sectors) {
      st1 = kNoData;
      break;
    }
    const size_t offset = ((static_cast<size_t>(cylinder) * disk->heads + head) * disk->sectors + (r - 1)) * kSectorSize;
    const size_t moved = dma_.Transfer(&disk->bytes[offset], kSectorSize, !write);
    if (write && moved) disk->modified = true;
    if (moved < kSectorSize && !dma_.TerminalCount()) {
      st1 = kOverrun;
      break;
    }
    const bool done = dma_.TerminalCount();
    if (r == eot) {
      r = 1;
      if (multitrack && head == 0) {
        head = 1;
        h = 1;
      } else {
        ++c;
        if (multitrack) h = 0;
        if (!done) st1 = kEndOfCylinder;
        break;
      }
    } else {
      ++r;
    }
    if (done) break;
  }
  if (st1) st0 = kAbnormal;
  st0 |= static_cast<uint8_t>(drive | head << 2);
  Result({st0, st1, st2, c, h, r, n});
  interrupt_ = true;
}
