// Boots the FreeDOS kernel on the emulated Aria with drive B: from a
// firmware update (HANDOFF 6.10, 6.11, bead ar-4cu) and checks how far it
// gets by what it writes through INT 10h. Prints PASS on success.
//
//   boot_test FIRMWARE.ZIP KERNEL.SYS [-v | -t] [cycles]
//
// -v prints the whole teletype output, the final registers and the ports
// the board model does not implement. -t also steps instruction by
// instruction and stops at the first jump into empty memory, listing the
// 40 instructions before it. -dump PATH saves the address space there, or
// at the end of the run without -t (it holds firmware, so never inside the
// repository); -dsp PATH saves the bytes sent to the DSP, likewise.

#include <algorithm>
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
  std::wstring dsp_path;   // Bytes sent to the DSP (0402h); outside the repository too.
  std::wstring dump_path;  // The 1 MB address space at the stop (-t) or the end, for analysis outside the repository.
  long long budget = 400'000'000;  // 84 s of the assumed 4.77 MHz.
  for (int i = 3; i < argc; ++i) {
    if (std::wstring(argv[i]) == L"-v") {
      verbose = true;
    } else if (std::wstring(argv[i]) == L"-t") {
      verbose = trace = true;
    } else if (std::wstring(argv[i]) == L"-dump" && i + 1 < argc) {
      dump_path = argv[++i];
    } else if (std::wstring(argv[i]) == L"-dsp" && i + 1 < argc) {
      dsp_path = argv[++i];
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
  std::vector<std::string> disk_log;
  if (verbose) machine.bios().SetDiskLog([&](const std::string& line) { disk_log.push_back(line); });
  machine.PowerOn();  long long ran = 0;
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
  if (!dsp_path.empty()) {
    std::ofstream out(std::filesystem::path(dsp_path), std::ios::binary);
    out.write(reinterpret_cast<const char*>(machine.DspBytes().data()),
              static_cast<std::streamsize>(machine.DspBytes().size()));
  }
  if (!trace && !dump_path.empty()) {
    std::ofstream out(std::filesystem::path(dump_path), std::ios::binary);
    for (uint32_t a = 0; a < 0x100000; ++a) out.put(static_cast<char>(machine.Peek(a)));
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
    const auto& dsp = machine.DspBytes();
    std::printf("DSP: %zu bytes to 0402h, %d writes to 0401h (last %02Xh); first bytes:", dsp.size(),
                machine.DspControlWrites(), machine.DspControl());
    for (std::size_t i = 0; i < dsp.size() && i < 32; ++i) std::printf(" %02X", dsp[i]);
    std::printf("\n");
    std::printf("cycles: %lld, BIOS ticks: %u\n", ran,
                machine.PeekWord(0x46C) | static_cast<unsigned>(machine.PeekWord(0x46E)) << 16);
    // PCB at I/O 0000h: timers 0 and 2, then the interrupt controller
    // (mask 28h, in-service 2Ch, request 2Eh, status 30h, timer control 32h).
    auto& cpu = machine.cpu();
    std::printf("PCB reloc %04X, T0 max %04X, T2 max %04X\n", cpu.RelocationRegister(), cpu.InWord(0x52),
                cpu.InWord(0x62));
    std::printf("PCB T0 count %04X ctl %04X, T2 count %04X ctl %04X, mask %04X isr %04X irr %04X st %04X tcr %04X\n",
                cpu.InWord(0x50), cpu.InWord(0x56), cpu.InWord(0x60), cpu.InWord(0x66), cpu.InWord(0x28),
                cpu.InWord(0x2C), cpu.InWord(0x2E), cpu.InWord(0x30), cpu.InWord(0x32));
    std::printf("INT 13h calls: %zu\n", disk_log.size());
    for (std::size_t i = 0; i < disk_log.size() && i < 60; ++i) std::printf("  %s\n", disk_log[i].c_str());
  }

  // Milestones in boot order, by what each stage prints (7.05, 7.04, 6.99
  // and 6.87 alike): the
  // kernel, then CONFIG.SYS from B: (VOICE.SYS, VDISK as D:), then
  // AUTOEXEC.BAT under ROM-DOS COMMAND.COM (AAPI, SPEAK, BKB).
  const char* milestones[] = {
      "FreeDOS kernel",
      "Voice device rev 1.3",
      "Installed as drive:  D:",
      "Aria API v1.1",  // 1.1b in 7.05, 1.1a in 6.87 and 6.99.
      "is now installed.",
      "ARIA keyboard driver",
      "Aria Alarm Scheduler",
      "Aria RunDOS Utility",
      ">mainmenu",
  };
  for (const char* milestone : milestones) {
    if (text.find(milestone) == std::string::npos) {
      std::cout << "FAIL boot: no \"" << milestone << "\" through INT 10h\n";
      return 1;
    }
  }

  // The DSP stand-in (HANDOFF 6.3): SPEAK boots the ADSP-2105 by streaming
  // LPC.DSP into 0402h, and the start-up harp (SYSTEM\SOUNDS\HARPUP.04)
  // follows after PLAY.DSP. Both are read from the loaded image, so the
  // check holds for any firmware build.
  Fat12Volume volume;
  std::vector<uint8_t> lpc, harp;
  if (!volume.Open(flash.bytes, error) || !volume.ReadFile("SYSTEM\\LPC.DSP", lpc) ||
      !volume.ReadFile("SYSTEM\\SOUNDS\\HARPUP.04", harp) || harp.size() < 64) {
    std::cout << "FAIL boot: cannot read LPC.DSP or HARPUP.04 from drive B:\n";
    return 1;
  }
  const auto& dsp = machine.DspBytes();
  if (dsp.size() < lpc.size() || !std::equal(lpc.begin(), lpc.end(), dsp.begin())) {
    std::cout << "FAIL boot: the DSP did not receive LPC.DSP first\n";
    return 1;
  }
  if (std::search(dsp.begin(), dsp.end(), harp.begin(), harp.begin() + 64) == dsp.end()) {
    std::cout << "FAIL boot: the DSP did not receive HARPUP.04\n";
    return 1;
  }
  std::cout << "PASS boot_test (CONFIG.SYS and AUTOEXEC.BAT from drive B: up to MAINMENU, LPC.DSP and harp to the DSP)\n";
  return 0;
}
