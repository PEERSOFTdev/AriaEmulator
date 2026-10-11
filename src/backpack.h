#ifndef ARIA_BACKPACK_H
#define ARIA_BACKPACK_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "parallel_port.h"

// A Micro Solutions Backpack floppy drive on the Aria's printer port
// (HANDOFF 6.7, ar-l5v), as BACKPACK.SYS ("BP v2.11.AR") talks to it. All
// offsets below are in BACKPACK.SYS the file.
//
// The drive's parallel-port chip is reached in nibble mode, as Linux's
// paride bpck mode 0 reaches its cousin:
//   - connect (+1460h): control 04h, data = unit number, SELECT IN raised
//     twice, then AUTOFEED high: the chip answers the unit number & 7 on
//     status bits 3-5, and inverted with AUTOFEED low; SELECT IN falling
//     ends it. A rising SELECT IN while connected disconnects (+1507h).
//   - address (+1542h): data, then AUTOFEED toggled.
//   - write (+1560h): data, STROBE high, INIT toggled, STROBE low; a block
//     (+1626h) keeps STROBE high and toggles INIT per byte.
//   - read (+158Bh): STROBE low, INIT toggled: low nibble on the status
//     lines; INIT toggled again: high nibble. Nibble bits 0-3 on PC status
//     bits 3, 4, 5, 7 (table +17C9h).
// Address 40h + n is register n of a PC floppy controller (82077 family)
// behind the chip; the rest are the chip's own: 28h and 2Ch the 3-byte
// out and in pointers of its buffer RAM, 30h a 3-byte count, 80h the
// buffer's data port (+16C0h-+17B6h).
class BackpackFloppy : public ParallelPort::Device {
 public:
  // `unit` is the number the driver probes, 0-99 (+3349h): the last two
  // digits of the serial number, by which README.DOC orders the letters.
  explicit BackpackFloppy(uint8_t unit);

  void Data(uint8_t value) override { data_ = value; }
  void Control(uint8_t pc_control) override;
  uint8_t Status() override;

  // Every register access, for working the chip out (boot_test -v).
  std::function<void(const std::string&)> log;

 private:
  enum class State { kIdle, kProbe, kConnected };

  uint8_t ReadRegister();
  void WriteRegister(uint8_t value);
  // Chip register 6 drives a 93C46 serial EEPROM (64 words of 16 bits),
  // bit-banged as Linux bpck reads its own: bit 0 SK, bit 1 DI, bit 2 CS,
  // bit 3 an enable around it (+1A19h-+1A6Ah); DO comes back on chip
  // register 0 bit 7 (+1A71h). Only READ is answered.
  void EepromControl(uint8_t value);
  void Log(const char* what, uint8_t reg, uint8_t value) const;

  uint8_t unit_;
  uint8_t data_ = 0;
  uint8_t control_ = 0x0C;
  State state_ = State::kIdle;
  int select_edges_ = 0;  // SELECT IN rising with our number on the data lines.

  uint8_t address_ = 0;
  int byte_index_ = 0;    // Within a multi-byte register since the address.
  bool high_nibble_ = false;
  uint8_t read_byte_ = 0;

  std::array<uint8_t, 0x40> chip_{};  // The chip's own one-byte registers.
  uint32_t out_ = 0, in_ = 0, count_ = 0;
  std::vector<uint8_t> buffer_ = std::vector<uint8_t>(0x20000);
  std::array<uint8_t, 8> fdc_{};  // Stub until the controller is emulated.

  std::array<uint16_t, 64> eeprom_{};
  uint8_t eeprom_pins_ = 0;
  int eeprom_bits_ = -1;        // Command bits after the start bit; -1 before it.
  uint16_t eeprom_command_ = 0;
  int eeprom_out_ = -1;         // Data bit being shifted out, 16 = the dummy 0.
  uint16_t eeprom_word_ = 0;
  uint8_t eeprom_address_ = 0;
  bool eeprom_do_ = false;
};

#endif
