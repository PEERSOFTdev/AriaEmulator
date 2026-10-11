#include "backpack.h"

#include <cstdio>

namespace {

constexpr uint8_t kStrobe = 0x01, kAutofeed = 0x02, kInit = 0x04, kSelectIn = 0x08;
// An idle port, as ParallelPort reads with nothing on it.
constexpr uint8_t kIdleStatus = 0x7F;
constexpr uint32_t kBufferMask = 0x1FFFF;  // 17-bit pointers (+1771h: AND DX,1).

// Status lines for nibble bits 0-3: PC status bits 3, 4, 5, 7 (+17C9h).
uint8_t NibbleStatus(uint8_t nibble) {
  return static_cast<uint8_t>(((nibble & 1) << 3) | ((nibble & 2) << 3) | ((nibble & 4) << 3) | ((nibble & 8) << 4));
}

uint32_t* Wide(uint8_t reg, uint32_t& out, uint32_t& in, uint32_t& count) {
  switch (reg) {
    case 0x28: return &out;
    case 0x2C: return &in;
    case 0x30: return &count;
    default: return nullptr;
  }
}

}  // namespace

BackpackFloppy::BackpackFloppy(uint8_t unit) : unit_(unit) {
  fdc_.log = [this](const std::string& line) {
    if (log) log(line);
  };
  eeprom_.fill(0xFFFF);
  // What BACKPACK.SYS reads (+318Ch-+3233h): word 0 low byte = NOT high
  // byte, or the drive is refused; word 1 high byte goes to chip register
  // 5; word 2 low byte (< 10h) is the controller type, 2 the 82077 with a
  // data rate register (+2290h); words 3-6 the drive positions, low byte
  // the drive type, FFh none; words 37h-3Ah the serial number, eight
  // ASCII digits (bpck prints the same place as its serial). Values found
  // by what the driver accepts and prints, not from a real drive's dump.
  eeprom_[0] = 0x00FF;
  eeprom_[1] = 0x0000;
  eeprom_[2] = 0x0002;
  // One drive, at position 1 (word 4), which is where the driver expects
  // the unit's own drive: FFh there still makes a drive, of the DRIVETYPE
  // parameter's type (+31ECh), while FFh at the other positions means none.
  // So the floppy is drive 1 on the controller. Type 7 is the eighth of
  // the driver's names, "3.5 inch 1.4MB" (+0AABh). With this the driver
  // says "The following BACKPACK drive is available: Drive E: - 3.5 inch
  // 1.4MB", one floppy unit being E: as README.DOC on drive B: has it.
  // The high byte is the drive's flags, as the driver's own default for
  // type 7 has them (43h, table +3656h, used for FFh at +31F1h): bit 6 a
  // disk-change line, so the media check reads DIR (+0676h AH=15h/16h,
  // +1E29h) instead of guessing "unchanged" for 37 ticks after the last
  // access (+099Bh); bits 0-4 index the step-rate table (+22AAh, +2A62h).
  eeprom_[4] = 0x4307;
  char serial[9];
  std::snprintf(serial, sizeof serial, "014187%02u", static_cast<unsigned>(unit % 100));
  for (int i = 0; i < 4; ++i) {
    eeprom_[0x37 + i] = static_cast<uint16_t>(static_cast<uint8_t>(serial[2 * i]) |
                                              static_cast<uint8_t>(serial[2 * i + 1]) << 8);
  }
}

void BackpackFloppy::EepromControl(uint8_t value) {
  const bool rising = (value & 1) && !(eeprom_pins_ & 1);
  eeprom_pins_ = value;
  if (!(value & 4)) {
    // Chip select low ends any command.
    eeprom_bits_ = -1;
    eeprom_out_ = -1;
    eeprom_do_ = false;
    return;
  }
  if (!rising) return;
  const int di = (value >> 1) & 1;
  if (eeprom_out_ >= 0) {
    // Shifting a word out, most significant bit first; past bit 0 the
    // next word follows, as a 93C46 reads on.
    eeprom_do_ = (eeprom_word_ >> eeprom_out_) & 1;
    if (--eeprom_out_ < 0) {
      eeprom_address_ = (eeprom_address_ + 1) & 0x3F;
      eeprom_word_ = eeprom_[eeprom_address_];
      eeprom_out_ = 15;
    }
    return;
  }
  if (eeprom_bits_ < 0) {
    if (di) eeprom_bits_ = 0;  // The start bit.
    eeprom_command_ = 0;
    return;
  }
  eeprom_command_ = static_cast<uint16_t>(eeprom_command_ << 1 | di);
  if (++eeprom_bits_ < 8) return;
  // Two opcode bits and six address bits; READ is 10b. The dummy 0 comes
  // first, then D15 on the next clock (+33F3h-+3409h).
  if ((eeprom_command_ >> 6) == 2) {
    eeprom_address_ = eeprom_command_ & 0x3F;
    eeprom_word_ = eeprom_[eeprom_address_];
    eeprom_out_ = 15;
    eeprom_do_ = false;
  } else {
    eeprom_bits_ = -1;
  }
}

void BackpackFloppy::Log(const char* what, uint8_t reg, uint8_t value) const {
  if (!log) return;
  char line[32];
  std::snprintf(line, sizeof line, "%s %02X %02X", what, reg, value);
  log(line);
}

void BackpackFloppy::Control(uint8_t pc_control) {
  const uint8_t changed = control_ ^ pc_control;
  control_ = pc_control;
  if ((changed & kSelectIn) && (pc_control & kSelectIn)) {
    // Rising SELECT IN: our number on the data lines arms the connect;
    // while connected it is the disconnect (+1507h).
    if (state_ == State::kConnected) {
      state_ = State::kIdle;
      if (log) log("disconnect");
    }
    select_edges_ = data_ == unit_ ? select_edges_ + 1 : 0;
    if (state_ == State::kIdle && select_edges_ >= 2) state_ = State::kProbe;
    return;
  }
  if ((changed & kSelectIn) && state_ == State::kProbe) {
    // Falling SELECT IN after the unit number was answered: connected.
    state_ = State::kConnected;
    select_edges_ = 0;
    address_ = 0;
    byte_index_ = 0;
    if (log) log("connect");
    return;
  }
  if (state_ != State::kConnected) return;
  if (changed & kAutofeed) {
    address_ = data_;
    byte_index_ = 0;
    high_nibble_ = false;
  }
  if (changed & kInit) {
    if (pc_control & kStrobe) {
      WriteRegister(data_);
    } else if (!high_nibble_) {
      read_byte_ = ReadRegister();
      high_nibble_ = true;
    } else {
      high_nibble_ = false;
    }
  }
}

uint8_t BackpackFloppy::Status() {
  switch (state_) {
    case State::kProbe: {
      // The unit number's low three bits on status bits 3-5, inverted
      // while AUTOFEED is low (+14BCh-+14E0h).
      const uint8_t id = (control_ & kAutofeed) ? (unit_ & 7) : (~unit_ & 7);
      return static_cast<uint8_t>((kIdleStatus & ~0x38) | (id << 3));
    }
    case State::kConnected:
      // After the first INIT edge of a byte the low nibble, after the
      // second the high one.
      return NibbleStatus(high_nibble_ ? (read_byte_ & 0x0F) : (read_byte_ >> 4));
    default:
      return kIdleStatus;
  }
}

uint8_t BackpackFloppy::ReadRegister() {
  const uint8_t reg = address_;
  uint8_t value = 0;
  if (uint32_t* wide = Wide(reg, out_, in_, count_)) {
    value = static_cast<uint8_t>(*wide >> (8 * byte_index_++));
  } else if (reg == 0x80) {
    value = buffer_[out_++ & kBufferMask];
  } else if (reg >= 0x40 && reg < 0x48) {
    value = fdc_.Read(reg - 0x40);
  } else if (reg == 0x00) {
    // Bit 2 low lets the driver empty the buffer while a read runs
    // (+1764h, +27F0h); bit 7 is the EEPROM's DO.
    value = static_cast<uint8_t>((chip_[0] & 0x7B) | (eeprom_do_ ? 0x80 : 0));
  } else if (reg == 0x0B) {
    value = static_cast<uint8_t>((chip_[0x0B] & 0x7F) | (fdc_.Interrupt() ? 0x80 : 0));
  } else {
    value = chip_[reg & 0x3F];
  }
  Log("R", reg, value);
  return value;
}

void BackpackFloppy::WriteRegister(uint8_t value) {
  const uint8_t reg = address_;
  Log("W", reg, value);
  if (uint32_t* wide = Wide(reg, out_, in_, count_)) {
    const int shift = 8 * byte_index_++;
    *wide = (*wide & ~(0xFFu << shift)) | static_cast<uint32_t>(value) << shift;
    if (reg == 0x30) dma_done_ = 0;
  } else if (reg == 0x80) {
    buffer_[in_++ & kBufferMask] = value;
  } else if (reg >= 0x40 && reg < 0x48) {
    fdc_.Write(reg - 0x40, value);
  } else {
    chip_[reg & 0x3F] = value;
    if (reg == 0x06) EepromControl(value);
  }
}

// The controller's DMA, served from the buffer: from the disk at the in
// pointer, to the disk from the out pointer, as the host's own 80h
// accesses use them, until count + 1 bytes have gone.
size_t BackpackFloppy::Transfer(uint8_t* data, size_t size, bool to_host) {
  const uint8_t mode = chip_[4];
  if (!(mode & 2) || static_cast<bool>(mode & 1) != to_host) return 0;
  size_t moved = 0;
  for (; moved < size && !TerminalCount(); ++moved, ++dma_done_) {
    if (to_host) {
      buffer_[in_++ & kBufferMask] = data[moved];
    } else {
      data[moved] = buffer_[out_++ & kBufferMask];
    }
  }
  return moved;
}
