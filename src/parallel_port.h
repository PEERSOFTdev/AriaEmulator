#ifndef ARIA_PARALLEL_PORT_H
#define ARIA_PARALLEL_PORT_H

#include <cstdint>
#include <functional>

// The Aria's printer port (HANDOFF 6.7), at I/O 680h-682h with its own
// wiring, not a PC's. Two programs from drive B: show it:
//
//   - AAPI's printer handler (AAPI.EXE+B5C8h, unpacked; AH 0/1/2 as INT 17h)
//     writes the character to 681h, strobes with 0Bh then 0Ah to 682h,
//     initialises with 02h then 0Ah, and reads the status at 680h.
//   - BACKPACK.SYS ("BP v2.11.AR", built for the Aria) talks to its drives
//     only through two helpers (file offsets +18DAh out, +18FCh in): data
//     out goes to 681h, status in comes from 681h, control out goes to
//     682h; nothing else answers. It translates through two tables in the
//     driver (+18C9h status, +19C9h control) between PC conventions and
//     the Aria's lines, and finds the port as LPT1 in the BIOS data area,
//     0040:0008h = 680h (+32FDh), which the missing boot ROM must have put
//     there.
//
// The tables are bit permutations, so the wiring reads off them:
//   control 682h: bit 0 STROBE, bit 1 SELECT IN, bit 2 AUTOFEED, bit 3 INIT
//     (PC control bits 0, 3, 1, 2), same sense;
//   status 680h/681h: bit 0 PAPER END, bit 1 BUSY, bit 2 ACK, bit 3 SELECT,
//     bit 4 ERROR (PC status bits 5, 7, 6, 4, 3), BUSY not inverted as the
//     PC's bit 7 is.
//
// A device behind the port (a Backpack drive, a printer) sees PC-style
// lines: data, the PC control byte and the PC status byte it answers with.
class ParallelPort {
 public:
  static constexpr uint16_t kBase = 0x0680;
  static constexpr uint16_t kPorts = 3;

  // What hangs on the port, in PC terms; nothing connected reads as an
  // idle, empty port.
  struct Device {
    virtual ~Device() = default;
    virtual void Data(uint8_t value) = 0;
    virtual void Control(uint8_t pc_control) = 0;
    virtual uint8_t Status() = 0;  // PC status byte, bits 3-7.
  };
  void Attach(Device* device) { device_ = device; }

  // `reg` is the offset from kBase.
  uint8_t Read(int reg);
  void Write(int reg, uint8_t value);

  // Every access, in PC terms, for working out a protocol (boot_test).
  std::function<void(int reg, uint8_t pc_value, bool write)> trace;

 private:
  uint8_t pc_control_ = 0x0C;  // INIT and SELECT IN, a printer at rest.
  Device* device_ = nullptr;
};

#endif
