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
// repository); -dsp PATH saves the bytes sent to the DSP, likewise, and
// -wav PATH RATE the samples the DSP shifted out, as a WAV file.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "aria_machine.h"
#include "hle_bios.h"
#include "snapshot.h"
#include "zip_reader.h"

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

// DOS's memory blocks (MCBs: 'M' or 'Z', owner, size in paragraphs, name).
// The chain is found by trying every start in the low memory and keeping
// the longest that ends in 'Z'; FreeDOS's own start (List of Lists) would
// need an INT 21h call from here.
struct Mcb {
  uint16_t segment, owner, paragraphs;
  std::string name;
};

std::vector<Mcb> McbChain(AriaMachine& m) {
  std::vector<Mcb> best;
  for (uint32_t start = 0x40; start < 0x2000; ++start) {
    std::vector<Mcb> chain;
    uint32_t p = start;
    while (p < 0xB000) {
      const uint8_t type = m.Peek(p * 16);
      if (type != 'M' && type != 'Z') break;
      Mcb block{static_cast<uint16_t>(p), m.PeekWord(p * 16 + 1), m.PeekWord(p * 16 + 3), {}};
      for (int i = 0; i < 8 && m.Peek(p * 16 + 8 + i); ++i) block.name.push_back(static_cast<char>(m.Peek(p * 16 + 8 + i)));
      chain.push_back(block);
      if (type == 'Z') {
        if (chain.size() > best.size()) best = chain;
        break;
      }
      p += block.paragraphs + 1u;
    }
  }
  return best;
}

// Paragraphs DOS must find free to start an EXE: its load image and
// minimum allocation from the MZ header, plus the PSP.
uint32_t ExeParagraphs(const std::vector<uint8_t>& exe) {
  auto word = [&](std::size_t at) { return static_cast<uint32_t>(exe[at] | exe[at + 1] << 8); };
  const uint32_t last = word(2), pages = word(4), header = word(8), minimum = word(10);
  const uint32_t image = pages * 512 - (last ? 512 - last : 0) - header * 16;
  return (image + 15) / 16 + minimum + 0x10;
}

// What the Aria says, as text: SPEAK's INT 61h with AX=0201h (a string at
// ES:DI, SPEAK.EXE+81B5h dispatch) and AX=0200h (one character in BL,
// SAY.COM). Once SPEAK owns vector 61h, the vector is pointed at a stub in
// the ROM's free space that traps to here and jumps on to SPEAK; the
// machine itself is not changed. Bytes are the image's encoding.
class SpeechLog {
 public:
  // After every slice: hooks the vector as soon as SPEAK has installed it.
  void Poll(AriaMachine& m) {
    if (hooked_) return;
    const uint16_t off = m.PeekWord(0x61 * 4), seg = m.PeekWord(0x61 * 4 + 2);
    if (seg == 0 || seg == 0xFFC0) return;
    hooked_ = true;
    // FFC0:0240h is unused by the BIOS's layout (hle_bios.cpp).
    const uint8_t stub[] = {0x0F, 0xF3, 0xEA, static_cast<uint8_t>(off), static_cast<uint8_t>(off >> 8),
                            static_cast<uint8_t>(seg), static_cast<uint8_t>(seg >> 8)};
    std::memcpy(m.RomStub() + 0x240, stub, sizeof stub);
    m.PokeWord(0x61 * 4, 0x0240);
    m.PokeWord(0x61 * 4 + 2, 0xFFC0);
    m.cpu().SetTrapHandler([this, &m](uint8_t number) {
      if (number != 0xF3) {
        m.bios().Trap(number);
        return;
      }
      const auto r = m.cpu().GetRegisters();
      if (r.ax == 0x0200) {
        text_.push_back(static_cast<char>(r.bx & 0xFF));
      } else if (r.ax == 0x0201) {
        const uint32_t at = (static_cast<uint32_t>(r.es) << 4) + r.di;
        for (uint32_t i = 0; i < 512 && m.Peek(at + i); ++i) text_.push_back(static_cast<char>(m.Peek(at + i)));
        text_.push_back('\n');
      }
    });
  }
  const std::string& text() const { return text_; }

 private:
  bool hooked_ = false;
  std::string text_;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 3) {
    std::cout << "FAIL usage: boot_test FIRMWARE KERNEL.SYS [-v] [cycles]\n";
    return 2;
  }
  bool verbose = false;
  bool trace = false;
  std::wstring dsp_path;   // Bytes sent to the DSP (0402h); outside the repository too.
  std::wstring wav_path;   // SPORT1 samples as a WAV file at `wav_rate`.
  uint32_t wav_rate = 8000;
  std::wstring dump_path;  // The 1 MB address space at the stop (-t) or the end, for analysis outside the repository.
  long long budget = AriaMachine::kCpuHz * 84LL;  // Emulated seconds.
  for (int i = 3; i < argc; ++i) {
    if (std::wstring(argv[i]) == L"-v") {
      verbose = true;
    } else if (std::wstring(argv[i]) == L"-t") {
      verbose = trace = true;
    } else if (std::wstring(argv[i]) == L"-dump" && i + 1 < argc) {
      dump_path = argv[++i];
    } else if (std::wstring(argv[i]) == L"-dsp" && i + 1 < argc) {
      dsp_path = argv[++i];
    } else if (std::wstring(argv[i]) == L"-wav" && i + 2 < argc) {
      wav_path = argv[++i];
      wav_rate = static_cast<uint32_t>(std::wcstoul(argv[++i], nullptr, 10));
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
  // Emulated seconds at which each stage first printed, for -v.
  const char* stages[] = {"FreeDOS kernel", "Voice device", "Installed as drive:  D:", "BACKPACK.SYS", "BPHDDRV.SYS",
                          ">rem", "CardTrick v1.04d", ">aapi", "Aria API v1.1", "is now installed.", ">bkb",
                          "Aria Alarm Scheduler", ">mainmenu"};
  std::vector<double> stage_seconds(std::size(stages), -1.0);
  double first_sound = -1.0;
  SpeechLog speech;
  while (ran < budget && !machine.PoweredOff() && !machine.bios().RebootRequested()) {
    machine.Run(kSlice);
    ran += kSlice;
    speech.Poll(machine);
    if (!verbose) continue;
    const double now = static_cast<double>(ran) / AriaMachine::kCpuHz;
    for (std::size_t s = 0; s < std::size(stages); ++s) {
      if (stage_seconds[s] < 0 && machine.bios().Teletype().find(stages[s]) != std::string::npos) stage_seconds[s] = now;
    }
    if (first_sound < 0) {
      for (int16_t v : machine.Samples()) {
        if (v != 0) {
          first_sound = now;
          break;
        }
      }
    }
  }
  // MAINMENU waits for a key now. Press "Where am I" (space + dot 4: port
  // 0581h bits 7 and 3, HANDOFF 6.4) the way the application does, held
  // 0.3 s, and see whether SPEAK sends new speech to the DSP.
  const std::size_t dsp_before = machine.DspBytes().size();
  const std::size_t samples_before = machine.Samples().size();
  machine.SetKeyRow(0, 0x88);
  for (long long t = 0; t < AriaMachine::kCpuHz * 3 / 10; t += kSlice) machine.Run(kSlice);
  machine.SetKeyRow(0, 0x00);
  for (long long t = 0; t < AriaMachine::kCpuHz * 15LL; t += kSlice) machine.Run(kSlice);
  const std::size_t key_dsp_bytes = machine.DspBytes().size() - dsp_before;
  const std::size_t key_samples = machine.Samples().size() - samples_before;
  if (verbose) {
    std::printf("after space+dot 4: %zu more bytes to the DSP, %zu more samples\n", key_dsp_bytes, key_samples);
    std::printf("emulated seconds to:");
    for (std::size_t s = 0; s < std::size(stages); ++s) std::printf(" [%s] %.1f", stages[s], stage_seconds[s]);
    std::printf(" [first sound] %.1f\n", first_sound);
  }
  if (!wav_path.empty()) {
    // 16-bit mono PCM at 44.1 kHz. The Aria's sample rate is unknown
    // (HANDOFF 6.3); `wav_rate` is the assumed rate at SCLKDIV 4 (Czech
    // LPC.DSP speech), and every sample lasts in proportion to its SCLKDIV + 1,
    // because the board derives the frame sync from the serial clock.
    // Sample-and-hold resampling: good enough to listen and compare.
    constexpr uint32_t kOutRate = 44100;
    const auto& samples = machine.Samples();
    const auto& dividers = machine.SampleDividers();
    std::vector<int16_t> rendered;
    double t_end = 0.0, t_out = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i) {
      t_end += (dividers[i] + 1) / 5.0 / wav_rate;
      for (; t_out < t_end; t_out += 1.0 / kOutRate) rendered.push_back(samples[i]);
    }
    const uint32_t data_bytes = static_cast<uint32_t>(rendered.size() * 2);
    std::ofstream out(std::filesystem::path(wav_path), std::ios::binary);
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) out.put(static_cast<char>(v >> (8 * i))); };
    auto u16 = [&](uint16_t v) { out.put(static_cast<char>(v)); out.put(static_cast<char>(v >> 8)); };
    out.write("RIFF", 4); u32(36 + data_bytes); out.write("WAVEfmt ", 8);
    u32(16); u16(1); u16(1); u32(kOutRate); u32(kOutRate * 2); u16(2); u16(16);
    out.write("data", 4); u32(data_bytes);
    for (int16_t s : rendered) u16(static_cast<uint16_t>(s));
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
    const auto& samples = machine.Samples();
    int peak = 0;
    for (int16_t s : samples) peak = std::max(peak, std::abs(static_cast<int>(s)));
    std::printf("DSP: %d boots, PC %04X%s, %zu samples out of SPORT1, peak %d, outside accesses %d\n",
                machine.DspBoots(), machine.dsp().Pc(), machine.dsp().Idle() ? " (IDLE)" : "", samples.size(), peak,
                machine.dsp().OutOfRangeAccesses());
    std::printf("DSP control: system 3FFFh %04X, SPORT1 3FF2h %04X, SCLKDIV 3FF1h %04X, autobuffer 3FEFh %04X\n",
                machine.dsp().ControlRegister(0x3FFF), machine.dsp().ControlRegister(0x3FF2),
                machine.dsp().ControlRegister(0x3FF1), machine.dsp().ControlRegister(0x3FEF));
    std::map<int, std::size_t> per_divider;
    for (uint16_t d : machine.SampleDividers()) ++per_divider[d];
    std::printf("DSP busy cycles %lld; samples per SCLKDIV:", static_cast<long long>(machine.dsp().BusyCycles()));
    for (const auto& [d, n] : per_divider) std::printf(" %d:%zu", d, n);
    std::printf("\n");
    for (const std::string& line : machine.DspLog()) std::printf("  %s", line.c_str());
    // Where the program changes (SCLKDIV changes), the samples around it.
    const auto& all = machine.Samples();
    const auto& divs = machine.SampleDividers();
    for (std::size_t i = 1; i < all.size(); ++i) {
      if (divs[i] == divs[i - 1]) continue;
      std::printf("SCLKDIV %d -> %d at sample %zu:", divs[i - 1], divs[i], i);
      for (std::size_t k = i >= 8 ? i - 8 : 0; k < all.size() && k < i + 72; ++k) std::printf(" %d", all[k]);
      std::printf("\n");
    }
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
  // The emulated ADSP-2105 runs both programs, and they use different
  // serial clock dividers: LPC.DSP speech (4 in the Czech builds, 6 in the
  // English 7.04) and PLAY.DSP's harp (19). So non-silent output at two
  // dividers means speech and a sound both came out of SPORT1.
  std::map<int, std::size_t> audible;
  for (std::size_t i = 0; i < machine.Samples().size(); ++i) {
    if (std::abs(static_cast<int>(machine.Samples()[i])) >= 16) ++audible[machine.SampleDividers()[i]];
  }
  int programs = 0;
  for (const auto& [divider, count] : audible) programs += count >= 1000;
  if (programs < 2) {
    std::cout << "FAIL boot: audible DSP output at fewer than two clock dividers:";
    for (const auto& [divider, count] : audible) std::cout << " " << divider << ":" << count;
    std::cout << "\n";
    return 1;
  }
  // The keyboard: BKB scans the matrix in timer 2's interrupt (vector 13h)
  // and MAINMENU answers "Where am I" with speech.
  if (key_dsp_bytes < 500) {
    std::cout << "FAIL boot: no speech after space + dot 4 (" << key_dsp_bytes << " bytes to the DSP)\n";
    return 1;
  }

  if (verbose) std::cout << "--- speech ---\n" << Printable(speech.text()) << "\n--- end ---\n";

  // Holds a key of the matrix for 0.3 s, then runs `seconds` more, or until
  // the machine switches itself off.
  auto press = [&](AriaMachine& m, int row, uint8_t bits, int seconds) {
    m.SetKeyRow(row, bits);
    for (long long t = 0; t < AriaMachine::kCpuHz * 3 / 10; t += kSlice) m.Run(kSlice);
    m.SetKeyRow(row, 0);
    for (long long t = 0; t < AriaMachine::kCpuHz * static_cast<long long>(seconds) && !m.PoweredOff(); t += kSlice)
      m.Run(kSlice);
  };

  // DOS memory as the Aria had it (HANDOFF 6.2, 9. 10. 2026): the kernel's
  // resident code in the ROM area, no "SC" block, and the largest free
  // block big enough for the word processor, AWP.EXE, which MAINMENU
  // starts on F1.
  std::vector<uint8_t> awp;
  if (!volume.ReadFile("SYSTEM\\AWP.EXE", awp) || awp.size() < 32) {
    std::cout << "FAIL boot: cannot read SYSTEM\\AWP.EXE from drive B:\n";
    return 1;
  }
  const std::vector<Mcb> chain = McbChain(machine);
  uint32_t largest = 0;
  bool sc = false;
  for (const Mcb& block : chain) {
    if (block.owner == 0) largest = std::max<uint32_t>(largest, block.paragraphs);
    if (block.owner == 8 && block.name == "SC") sc = true;
  }
  if (verbose) {
    for (const Mcb& block : chain)
      std::printf("MCB %04X owner %04X %6u B %s\n", block.segment, block.owner, block.paragraphs * 16u, block.name.c_str());
    std::printf("kernel in ROM: %s; largest free %u B, AWP needs %u B\n", machine.bios().KernelInRom() ? "yes" : "no",
                largest * 16, ExeParagraphs(awp) * 16);
  }
  if (!machine.bios().KernelInRom() || sc || chain.empty()) {
    std::cout << "FAIL boot: the kernel's resident code is not in the ROM area\n";
    return 1;
  }
  if (largest < ExeParagraphs(awp)) {
    std::cout << "FAIL boot: " << largest * 16 << " B free, AWP.EXE needs " << ExeParagraphs(awp) * 16 << " B\n";
    return 1;
  }

  // A cold start finds no signature of MAINMENU's at B000:0030 in the RAM
  // disk's boot sector (an 8-byte compare, MAINMENU.EXE+0729h-+0796h), so
  // MAINMENU asks IDS_MM_RAMDISK_CORRUPT "ram disk může být poškozen,
  // formátovat, ano nebo ne?" and takes every key until it is answered.
  // The yes characters are "yY" in the Czech builds too (MESSAGES.DAT,
  // IDS_YES_CHARS); y is dots 1, 3, 4, 5, 6 = 0581h bits 2, 0, 3, 4, 5.
  if (speech.text().find(", ano nebo ne?") == std::string::npos) {
    std::cout << "FAIL boot: no RAM disk question after a cold start\n";
    return 1;
  }
  std::size_t mark = speech.text().size();
  press(machine, 0, 0x3D, 8);
  if (speech.text().find(" ok", mark) == std::string::npos) {
    std::cout << "FAIL boot: y did not format drive D: (" << Printable(speech.text().substr(mark)) << ")\n";
    return 1;
  }

  // The battery keeps the RAM disk D: through power-off (HANDOFF 6.9), and
  // the emulator keeps it between runs (snapshot.h): plant a file entry in
  // D:'s root directory, switch off the Aria's own way, save to a snapshot
  // file, load it into a new machine, power that on, and see that
  // RAMDISK.EXE takes the disk over and MAINMENU does not ask again. VDISK
  // sits at B0000h (RAMDISK.EXE+01BFh, HANDOFF 6.2), its boot sector
  // carries "VDISK" and a BPB.
  constexpr uint32_t kRamDisk = 0xB0000;
  auto vdisk = [](AriaMachine& m) {
    const char sign[] = "VDISK";
    for (int i = 0; i < 5; ++i) {
      if (m.Peek(kRamDisk + 3 + i) != static_cast<uint8_t>(sign[i])) return false;
    }
    return true;
  };
  if (!vdisk(machine)) {
    std::cout << "FAIL boot: no VDISK boot sector at B0000h\n";
    return 1;
  }
  const uint32_t sector = machine.PeekWord(kRamDisk + 0x0B);
  const uint32_t root = kRamDisk + sector * (machine.PeekWord(kRamDisk + 0x0E) +
                                             machine.Peek(kRamDisk + 0x10) * machine.PeekWord(kRamDisk + 0x16));
  const char kMarker[] = "BATERIE TXT";
  uint32_t entry = root;
  while (machine.Peek(entry) != 0 && machine.Peek(entry) != 0xE5) entry += 32;
  for (int i = 0; i < 11; ++i) machine.Poke(entry + i, static_cast<uint8_t>(kMarker[i]));
  machine.Poke(entry + 11, 0x20);  // Archive.
  // "Konec": all four cursor keys (0584h bits 0-3) are K_UDLR 18Fh, on which
  // MAINMENU says message 8 "vypnuto" and calls Power(POWER_SWITCH_OFF)
  // (MAINMENU.EXE+087Ah, +09AFh); AAPI waits for the speech to end and
  // writes 0 to 0701h (AAPI.EXE+DDF0h).
  mark = speech.text().size();
  press(machine, 2, 0x0F, 20);
  if (!machine.PoweredOff() || speech.text().find("vypnuto", mark) == std::string::npos) {
    std::cout << "FAIL boot: konec (all four cursor keys) did not say vypnuto and switch off\n";
    return 1;
  }

  const uint32_t firmware = Crc32(flash.bytes.data(), flash.bytes.size());
  const std::filesystem::path memory_file = std::filesystem::temp_directory_path() / "aria_boot_test_pamet.bin";
  std::vector<uint8_t> state, loaded;
  std::wstring snapshot_error;
  machine.SaveState(state);
  const bool saved = snapshot::Save(memory_file, firmware, state, snapshot_error);
  const snapshot::Result read =
      snapshot::Load(memory_file, firmware, AriaMachine::kStateSize, loaded, snapshot_error);
  std::error_code ec;
  std::filesystem::remove(memory_file, ec);
  AriaMachine second(flash, kernel, [] { return int64_t{946728000}; });
  if (!saved || read != snapshot::Result::kOk || !second.LoadState(loaded)) {
    std::cout << "FAIL boot: the memory snapshot did not survive a round trip\n";
    return 1;
  }
  second.PowerOn();
  SpeechLog second_speech;
  long long after_menu = -1;  // Cycles run since ">mainmenu", for MAINMENU's own start.
  for (long long t = 0; t < AriaMachine::kCpuHz * 40LL && after_menu < AriaMachine::kCpuHz * 10LL; t += kSlice) {
    second.Run(kSlice);
    second_speech.Poll(second);
    if (after_menu >= 0) {
      after_menu += kSlice;
    } else if (second.bios().Teletype().find(">mainmenu") != std::string::npos) {
      after_menu = 0;
    }
  }
  if (verbose) {
    std::cout << "--- INT 10h output, second power-on ---\n" << Printable(second.bios().Teletype()) << "\n";
    std::cout << "--- speech, second power-on ---\n" << Printable(second_speech.text()) << "\n--- end ---\n";
  }
  bool kept = vdisk(second);
  for (int i = 0; i < 11 && kept; ++i) kept = second.Peek(entry + i) == static_cast<uint8_t>(kMarker[i]);
  if (after_menu < 0 || !kept) {
    std::cout << "FAIL boot: after a second power-on " << (kept ? "no MAINMENU" : "drive D: was made anew") << "\n";
    return 1;
  }
  if (second_speech.text().find(", ano nebo ne?") != std::string::npos) {
    std::cout << "FAIL boot: MAINMENU asks to format drive D: again after a power-on with the memory kept\n";
    return 1;
  }
  // F1 starts the word processor; without the memory ROM-DOS left free,
  // MAINMENU said "plná paměť" and "please disable old flash card support
  // in mode" instead (MAINMENU.EXE+0351h-+0372h, HANDOFF 6.2).
  mark = second_speech.text().size();
  press(second, 1, 0x01, 10);
  if (verbose) std::cout << "--- speech after F1 ---\n" << Printable(second_speech.text().substr(mark)) << "\n--- end ---\n";
  bool awp_running = false;
  for (const Mcb& block : McbChain(second)) awp_running = awp_running || block.name == "AWP";
  if (!awp_running || second_speech.text().find("please disable", mark) != std::string::npos) {
    std::cout << "FAIL boot: F1 did not start the word processor (" << Printable(second_speech.text().substr(mark))
              << ")\n";
    return 1;
  }
  std::cout << "PASS boot_test (CONFIG.SYS and AUTOEXEC.BAT from drive B: up to MAINMENU, speech and harp from the DSP,"
               " space + dot 4 answered, D: formatted on y, konec says vypnuto and switches off, D: kept through"
               " a memory snapshot without a new question, F1 starts the word processor)\n";
  return 0;
}
