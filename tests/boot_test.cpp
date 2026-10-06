// Boots the FreeDOS kernel on the emulated Aria with drive B: from a
// firmware update (HANDOFF 6.10, 6.11, bead ar-4cu) and checks how far it
// gets by what it writes through INT 10h. Prints PASS on success.
//
//   boot_test FIRMWARE.ZIP KERNEL.SYS [-v | -t] [cycles]
//
// -v prints the whole teletype output, the final registers and the ports
// the board model does not implement. -t also steps instruction by
// instruction and stops at the first jump into empty memory, listing the
// 40 instructions before it; with -dump PATH it then saves the address
// space (it holds firmware, so never inside the repository).

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "aria_machine.h"
#include "hle_bios.h"

namespace {

std::string Printable(const std::string& bytes) {
  std::string out;
  for (unsigned char ch : bytes) {
    if (ch == '\r') continue;
    if (ch == '\n' || (ch >= 0x20 && ch < 0x7F)) {
      out.push_back(static_cast<char>(ch));
    } else {
      char hex[8];
      std::snprintf(hex, sizeof hex, "<%02X>", ch);
      out += hex;
    }
  }
  return out;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 3) {
    std::cout << "FAIL usage: boot_test FIRMWARE KERNEL.SYS [-v] [cycles]\n";
    return 2;
  }
  bool verbose = false;
  bool trace = false;
  std::wstring dump_path;  // -t: the 1 MB address space at the stop, for analysis outside the repository.
  long long budget = 400'000'000;  // 50 s of the assumed 8 MHz.
  for (int i = 3; i < argc; ++i) {
    if (std::wstring(argv[i]) == L"-v") {
      verbose = true;
    } else if (std::wstring(argv[i]) == L"-t") {
      verbose = trace = true;
    } else if (std::wstring(argv[i]) == L"-dump" && i + 1 < argc) {
      dump_path = argv[++i];
    } else {
      budget = std::wcstoll(argv[i], nullptr, 10);
    }
  }

  FlashImage flash;
  std::wstring error;
  if (!LoadFlashImage(argv[1], TextEncoding::kUnknown, flash, error)) {
    std::wcout << L"FAIL firmware: " << error << L"\n";
    return 1;
  }
  std::ifstream file(argv[2], std::ios::binary);
  std::vector<uint8_t> kernel((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (kernel.size() < 1024) {
    std::cout << "FAIL kernel: cannot read KERNEL.SYS\n";
    return 1;
  }

  // A fixed clock keeps the run deterministic: 1.1.2000 12:00.
  AriaMachine machine(flash, kernel, [] { return int64_t{946728000}; });
  machine.PowerOn();
  long long ran = 0;
  if (trace) {
    // Instruction by instruction, stopping where execution runs into
    // empty memory (00 00 00 00): the mark of a wild jump. The last
    // instructions before it show where it came from.
    struct Seen { uint16_t cs, ip; uint8_t bytes[6]; };
    std::vector<Seen> ring(40);
    std::size_t at = 0;
    while (ran < budget && !machine.PoweredOff()) {
      const auto r = machine.cpu().GetRegisters();
      const uint32_t linear = (static_cast<uint32_t>(r.cs) << 4) + r.ip;
      Seen& s = ring[at++ % ring.size()];
      s.cs = r.cs;
      s.ip = r.ip;
      for (int i = 0; i < 6; ++i) s.bytes[i] = machine.Peek(linear + i);
      if ((s.bytes[0] | s.bytes[1] | s.bytes[2] | s.bytes[3]) == 0) break;
      ran += machine.cpu().Step();
    }
    for (std::size_t i = 0; i < ring.size(); ++i) {
      const Seen& s = ring[(at + i) % ring.size()];
      std::printf("%04X:%04X  %02X %02X %02X %02X %02X %02X\n", s.cs, s.ip, s.bytes[0], s.bytes[1],
                  s.bytes[2], s.bytes[3], s.bytes[4], s.bytes[5]);
    }
    // Which 1 KB blocks of the low 128 KB hold anything, to see how far a
    // decompressor wrote.
    std::printf("non-empty 1 KB blocks below 20000h:");
    for (uint32_t block = 0; block < 0x20000; block += 0x400) {
      bool any = false;
      for (uint32_t a = block; a < block + 0x400 && !any; ++a) any = machine.Peek(a) != 0;
      if (any) std::printf(" %05X", block);
    }
    std::printf("\n");
    if (!dump_path.empty()) {
      std::ofstream out(std::filesystem::path(dump_path), std::ios::binary);
      for (uint32_t a = 0; a < 0x100000; ++a) out.put(static_cast<char>(machine.Peek(a)));
    }
  }
  constexpr int kSlice = 100'000;
  while (ran < budget && !machine.PoweredOff() && !machine.bios().RebootRequested()) {
    machine.Run(kSlice);
    ran += kSlice;
  }

  const std::string text = machine.bios().Teletype();
  if (verbose) {
    std::cout << "--- INT 10h output ---\n" << Printable(text) << "\n--- end ---\n";
    const auto r = machine.cpu().GetRegisters();
    std::printf("CS:IP %04X:%04X SS:SP %04X:%04X AX %04X BX %04X CX %04X DX %04X DS %04X ES %04X%s\n",
                r.cs, r.ip, r.ss, r.sp, r.ax, r.bx, r.cx, r.dx, r.ds, r.es,
                machine.cpu().Halted() ? " (HLT)" : "");
    for (const auto& [port, count] : machine.UnknownPorts()) {
      std::printf("port %04Xh: %d accesses\n", port, count);
    }
    std::printf("cycles: %lld\n", ran);
  }

  // The kernel prints its version banner once its init has the disks.
  if (text.find("FreeDOS kernel") == std::string::npos) {
    std::cout << "FAIL boot: no FreeDOS banner through INT 10h\n";
    return 1;
  }
  std::cout << "PASS boot_test (FreeDOS kernel banner from drive B:)\n";
  return 0;
}
