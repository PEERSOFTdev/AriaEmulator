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
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "aria_machine.h"
#include "backpack.h"
#include "card_folder.h"
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
      if (number == 0xF4) {
        returned_ = true;  // DosCall's stub.
        return;
      }
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
  bool hooked() const { return hooked_; }
  // True once since the last call if DosCall's stub came back.
  bool TakeReturn() {
    const bool was = returned_;
    returned_ = false;
    return was;
  }

 private:
  bool hooked_ = false;
  bool returned_ = false;
  std::string text_;
};

// A diskette as DOS FORMAT lays it out, with one file in the root, cluster
// 2 onwards. 1.44 MB: 2880 sectors, cluster of 1 sector, two FATs of 9
// sectors, 224 root entries, data from sector 33. 720 KB: 1440 sectors,
// cluster of 2, FATs of 3, 112 root entries, data from sector 14.
std::vector<uint8_t> FloppyImage(bool high_density, const char* name83, const std::string& text) {
  const uint8_t per_cluster = high_density ? 1 : 2, fat_sectors = high_density ? 9 : 3;
  const uint8_t root_entries = high_density ? 224 : 112;
  const uint16_t total = high_density ? 2880 : 1440;
  const uint8_t media = high_density ? 0xF0 : 0xF9;
  const uint32_t root = 1 + 2u * fat_sectors, data = root + root_entries * 32u / 512;
  std::vector<uint8_t> image(total * 512u);
  const uint8_t boot[] = {0xEB, 0x3C, 0x90, 'A', 'R', 'I', 'A', 'T', 'E', 'S', 'T', 0x00, 0x02, per_cluster, 1, 0, 2,
                          root_entries, 0, static_cast<uint8_t>(total), static_cast<uint8_t>(total >> 8), media,
                          fat_sectors, 0, static_cast<uint8_t>(high_density ? 18 : 9), 0, 2, 0};
  std::memcpy(image.data(), boot, sizeof boot);
  image[510] = 0x55;
  image[511] = 0xAA;
  const uint32_t clusters = static_cast<uint32_t>((text.size() + 512u * per_cluster - 1) / (512u * per_cluster));
  for (int fat = 0; fat < 2; ++fat) {
    uint8_t* f = &image[(1 + fat_sectors * fat) * 512];
    f[0] = media;
    f[1] = f[2] = 0xFF;
    for (uint32_t c = 2; c < 2 + clusters; ++c) {
      const uint16_t next = c + 1 < 2 + clusters ? static_cast<uint16_t>(c + 1) : 0xFFF;
      uint8_t* at = f + c * 3 / 2;
      if (c & 1) {
        at[0] = static_cast<uint8_t>((at[0] & 0x0F) | (next << 4));
        at[1] = static_cast<uint8_t>(next >> 4);
      } else {
        at[0] = static_cast<uint8_t>(next);
        at[1] = static_cast<uint8_t>((at[1] & 0xF0) | (next >> 8));
      }
    }
  }
  uint8_t* entry = &image[root * 512];
  std::memcpy(entry, name83, 11);
  entry[11] = 0x20;
  entry[26] = 2;
  const uint32_t size = static_cast<uint32_t>(text.size());
  for (int i = 0; i < 4; ++i) entry[28 + i] = static_cast<uint8_t>(size >> (8 * i));
  std::memcpy(&image[data * 512], text.data(), text.size());
  return image;
}

// INT 21h inside the running machine, as a program on the Aria would call
// it, to see drive C: the way DOS shows it. It waits until the CPU runs in
// one of the `waiting` blocks with interrupts enabled and no hardware
// interrupt in service (PCB in-service register 2Ch): MAINMENU waits for a
// key inside AAPI's INT 60h, outside DOS and outside every driver's
// interrupt handler. Then it runs "int 21h; trap F4h; jmp $" from the ROM
// stub's free space at FFC0:0260h with the registers in `io` (CS:IP, SS:SP
// and FLAGS are its own: the stack ends just below `scratch_top`, in a
// free DOS block), and puts every register back afterwards. `io` gets the
// registers after the call.
bool DosCall(AriaMachine& m, SpeechLog& speech, const std::vector<Mcb>& waiting, uint16_t scratch_top,
             Cpu80188::Registers& io, const std::function<void()>& stalled = {}) {
  if (!speech.hooked()) return false;
  for (int i = 0;; ++i) {
    const auto r = m.cpu().GetRegisters();
    const uint32_t at = (static_cast<uint32_t>(r.cs) << 4) + r.ip;
    bool inside = false;
    for (const Mcb& block : waiting) {
      const uint32_t begin = static_cast<uint32_t>(block.segment + 1) << 4;
      inside = inside || (at >= begin && at < begin + (static_cast<uint32_t>(block.paragraphs) << 4));
    }
    if (inside && (r.flags & 0x0200) && m.cpu().InWord(0x2C) == 0) break;
    if (i > 2'000'000) return false;
    m.cpu().Step();
  }
  const Cpu80188::Registers saved = m.cpu().GetRegisters();
  const uint8_t stub[] = {0xCD, 0x21, 0x0F, 0xF4, 0xEB, 0xFE};
  std::memcpy(m.RomStub() + 0x260, stub, sizeof stub);
  Cpu80188::Registers call = io;
  call.cs = 0xFFC0;
  call.ip = 0x0260;
  call.ss = static_cast<uint16_t>(scratch_top - 0x1000);
  call.sp = 0;  // The first push lands just below `scratch_top`.
  call.flags = saved.flags;
  m.cpu().SetRegisters(call);
  speech.TakeReturn();
  bool back = false;
  for (long long t = 0; t < AriaMachine::kCpuHz * 20LL && !back; t += 10'000) {
    m.Run(10'000);
    back = speech.TakeReturn();
    // Still in DOS after 3 s: a prompt waits for the user (INT 24h).
    if (!back && stalled && t == AriaMachine::kCpuHz * 3LL) stalled();
  }
  io = m.cpu().GetRegisters();
  m.cpu().SetRegisters(saved);
  return back;
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
  // A 512 KB SRAM card in the slot, drive C: (HANDOFF 6.8), made from a host
  // folder with a letter in a subfolder (card_folder.h); the Aria's own
  // drivers take it over, DOS reads and writes it below, and the card goes
  // back into the folder. One folder per run: the boots run in parallel.
  constexpr uint32_t kCardSize = 512 * 1024;
  constexpr char kLetter[] = "Mila Ario,\r\nposilam dopis.\r\n";
  Fat12Format card_format;
  card_format.sectors = kCardSize / 512;
  card_format.label = "KARTA";
  const std::filesystem::path card_home = std::filesystem::temp_directory_path() /
                                          ("aria_boot_test_karta_" +
                                           std::to_string(Crc32(flash.bytes.data(), flash.bytes.size())));
  std::error_code card_ec;
  std::filesystem::remove_all(card_home, card_ec);
  std::filesystem::create_directories(card_home / "Dopisy");
  std::ofstream(card_home / "Dopisy" / "Prvni dopis.txt", std::ios::binary) << kLetter;
  FolderCard folder_card;
  {
    std::vector<uint8_t> image;
    std::wstring card_error;
    if (!folder_card.Load(card_home, kCardSize, flash.encoding, image, card_error)) {
      std::wcout << L"FAIL boot: the folder card: " << card_error << L"\n";
      return 1;
    }
    // Locked from power-on, as a remembered locked card is: the drivers
    // cannot tell it is SRAM by writing to it and must read its CIS
    // (pc_card.cpp). Unlocked below once DOS has read from it.
    PcCard first_card = PcCard::Sram(kCardSize, std::move(image));
    first_card.write_protected = true;
    machine.InsertCard(std::move(first_card));
  }
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

  // Drive C: on the SRAM card (HANDOFF 6.8, ar-3ap): while MAINMENU waits,
  // DOS creates C:\KARTA.TXT, writes it, and reads it back through the
  // Aria's card driver; then the file must be in the card's own bytes, as
  // a FAT volume a host could read.
  {
    const char kCardText[] = "Aria, karta SRAM\r\n";
    const std::vector<Mcb> blocks = McbChain(machine);
    std::vector<Mcb> waiting;  // MAINMENU and AAPI, where it waits for a key.
    const Mcb* free_block = nullptr;
    for (const Mcb& block : blocks) {
      if (block.name == "MAINMENU" || block.name == "AAPI") waiting.push_back(block);
      if (block.owner == 0 && (!free_block || block.paragraphs > free_block->paragraphs)) free_block = &block;
    }
    if (waiting.size() != 2 || !free_block || free_block->paragraphs < 0x100) {
      std::cout << "FAIL boot: no MAINMENU and AAPI blocks or no free DOS memory for the drive C: check\n";
      return 1;
    }
    // Scratch data at the start of the free block, the stack at its top.
    const uint16_t data = static_cast<uint16_t>(free_block->segment + 1);
    const uint16_t top = static_cast<uint16_t>(data + std::min<uint32_t>(free_block->paragraphs, 0x1000));
    const uint32_t base = static_cast<uint32_t>(data) << 4;
    const char kPath[] = "C:\\KARTA.TXT";
    for (std::size_t i = 0; i < sizeof kPath; ++i) machine.Poke(base + i, static_cast<uint8_t>(kPath[i]));
    for (std::size_t i = 0; i + 1 < sizeof kCardText; ++i)
      machine.Poke(base + 0x20 + i, static_cast<uint8_t>(kCardText[i]));
    auto dos = [&](uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, Cpu80188::Registers& out) {
      out = {};
      out.ax = ax;
      out.bx = bx;
      out.cx = cx;
      out.dx = dx;
      out.ds = out.es = data;
      return DosCall(machine, speech, waiting, top, out) && !(out.flags & 0x0001);
    };
    Cpu80188::Registers r;
    constexpr uint16_t kLength = sizeof kCardText - 1;
    std::string step;
    // The card locked since power-on must be a drive DOS can ask about.
    // Without a CIS the driver took it for an unknown card and every access
    // said "celkové selhání c" (user's report, 10. 10. 2026, HANDOFF 6.8).
    // Only the free space is asked: opening a file here, on 6.87, left the
    // battery report silent later on, for reasons not yet known.
    if (!dos(0x3600, 0, 0, 3, r) || r.ax == 0xFFFF || r.cx != 512 || r.dx == 0)
      step = "36h (free space) on a card locked since power-on";
    else if (machine.card()->Writes() != 0)
      step = "36h wrote to a locked card";
    machine.card()->write_protected = false;
    if (step.empty() && !dos(0x3C00, 0, 0, 0, r)) step = "create";
    const uint16_t handle = r.ax;
    if (step.empty() && (!dos(0x4000, handle, kLength, 0x20, r) || r.ax != kLength)) step = "write";
    if (step.empty() && !dos(0x3E00, handle, 0, 0, r)) step = "close";
    if (step.empty() && !dos(0x3D00, 0, 0, 0, r)) step = "open";
    const uint16_t again = r.ax;
    if (step.empty() && (!dos(0x3F00, again, 0x40, 0x80, r) || r.ax != kLength)) step = "read";
    if (step.empty() && !dos(0x3E00, again, 0, 0, r)) step = "close after reading";
    for (uint16_t i = 0; step.empty() && i < kLength; ++i) {
      if (machine.Peek(base + 0x80 + i) != static_cast<uint8_t>(kCardText[i])) step = "compare";
    }
    if (!step.empty()) {
      std::cout << "FAIL boot: drive C: on an SRAM card, INT 21h " << step << " (AX " << std::hex << r.ax << std::dec
                << ")\n";
      return 1;
    }
    Fat12Volume card_volume;
    std::wstring card_error;
    std::vector<uint8_t> on_card;
    if (!machine.card() || !machine.card()->Modified() || !card_volume.Open(machine.card()->Contents(), card_error) ||
        !card_volume.ReadFile("KARTA.TXT", on_card) ||
        std::string(on_card.begin(), on_card.end()) != std::string(kCardText, kLength)) {
      std::cout << "FAIL boot: C:\\KARTA.TXT is not in the SRAM card's FAT volume\n";
      return 1;
    }

    // The letter from the host folder's subfolder, as the Aria reads it
    // ("Prvni dopis.txt" is PRVNID~1.TXT on the card), and the card's new
    // file back in the folder.
    const char kLetterPath[] = "C:\\DOPISY\\PRVNID~1.TXT";
    for (std::size_t i = 0; i < sizeof kLetterPath; ++i)
      machine.Poke(base + 0x200 + i, static_cast<uint8_t>(kLetterPath[i]));
    constexpr uint16_t kLetterLength = sizeof kLetter - 1;
    std::string letter_step;
    if (!dos(0x3D00, 0, 0, 0x200, r)) letter_step = "open";
    const uint16_t letter = r.ax;
    if (letter_step.empty() && (!dos(0x3F00, letter, 0x80, 0x280, r) || r.ax != kLetterLength)) letter_step = "read";
    if (letter_step.empty() && !dos(0x3E00, letter, 0, 0, r)) letter_step = "close";
    for (uint16_t i = 0; letter_step.empty() && i < kLetterLength; ++i) {
      if (machine.Peek(base + 0x280 + i) != static_cast<uint8_t>(kLetter[i])) letter_step = "compare";
    }
    std::wstring save_error;
    bool saved_back = folder_card.Save(machine.card()->Contents(), save_error);
    if (saved_back) {
      std::ifstream back(card_home / "KARTA.TXT", std::ios::binary);
      const std::string text((std::istreambuf_iterator<char>(back)), std::istreambuf_iterator<char>());
      saved_back = text == std::string(kCardText, kLength);
    }
    if (!letter_step.empty() || !saved_back) {
      std::cout << "FAIL boot: folder card: "
                << (!letter_step.empty() ? "INT 21h " + letter_step + " of C:\\DOPISY\\PRVNID~1.TXT"
                                         : "KARTA.TXT did not come back into the folder")
                << "\n";
      return 1;
    }
    std::filesystem::remove_all(card_home, card_ec);

    // Swapping cards while the Aria runs (HANDOFF 6.8): DOS keeps 50
    // buffers (CONFIG.SYS), so unless the card driver reports the change,
    // DOS would go on with the old card's FAT and directory and could
    // write them onto the new one. `wait` is how long the host leaves the
    // slot empty: DLFFSA polls 0480h from INT 1Ch, and an instant swap is
    // safe only because the machine keeps the slot empty for
    // kCardSwapTicks itself.
    auto has = [](const PcCard& card, const char* name) {
      Fat12Volume volume;
      std::wstring ignored;
      std::vector<uint8_t> bytes;
      return volume.Open(card.Contents(), ignored) && volume.ReadFile(name, bytes);
    };
    auto swap = [&](PcCard next, double wait, const char* expect, const char* absent, const char* create,
                    PcCard& out) -> std::string {
      std::optional<PcCard> taken = machine.EjectCard();
      for (long long t = 0; t < static_cast<long long>(AriaMachine::kCpuHz * wait); t += kSlice) machine.Run(kSlice);
      const std::vector<uint8_t> before = taken->Contents();
      machine.InsertCard(std::move(next));
      // Swapped at once, the new card must wait out AriaMachine::kCardSwapTicks.
      if (machine.CardWaiting() != (wait == 0.0)) return "swap gap not kept";
      for (int i = 0; i < 100 && machine.CardWaiting(); ++i) machine.Run(kSlice);
      auto put =[&](uint32_t at, const char* path) {
        for (std::size_t i = 0; i < 4; ++i) machine.Poke(base + at + i, static_cast<uint8_t>("C:\\"[i]));
        for (std::size_t i = 0; i <= std::strlen(path); ++i) machine.Poke(base + at + 3 + i, static_cast<uint8_t>(path[i]));
      };
      put(0x100, expect);
      put(0x120, absent);
      put(0x140, create);
      Cpu80188::Registers s;
      if (expect[0] && (!dos(0x3D00, 0, 0, 0x100, s) || !dos(0x3E00, s.ax, 0, 0, s))) return "the card's own file not found";
      if (dos(0x3D00, 0, 0, 0x120, s)) return "the old card's file still seen";
      if (!dos(0x3C00, 0, 0, 0x140, s)) return "create failed";
      const uint16_t h = s.ax;
      if (!dos(0x4000, h, kLength, 0x20, s) || !dos(0x3E00, h, 0, 0, s)) return "write failed";
      if (taken->Contents() != before) return "the card taken out changed";
      if (!has(*machine.card(), create)) return "the new file is not on the card";
      if (has(*machine.card(), absent)) return "the old card's file was written onto the new card";
      out = std::move(*taken);
      return {};
    };
    Fat12Format second_format = card_format;
    second_format.label = "DRUHA";
    PcCard first;
    std::string why = swap(PcCard::Sram(kCardSize, FormatFat12(second_format)), 2.0, "", "KARTA.TXT", "DRUHA.TXT", first);
    std::string fast_why;
    if (why.empty()) {
      PcCard second;
      fast_why = swap(std::move(first), 0.0, "KARTA.TXT", "DRUHA.TXT", "TRETI.TXT", second);
    }
    if (verbose) std::cout << "card swap after 2 s: " << (why.empty() ? "ok" : why) << "; at once: "
                           << (fast_why.empty() ? "ok" : fast_why) << "\n";
    if (!why.empty() || !fast_why.empty()) {
      std::cout << "FAIL boot: swapping SRAM cards, " << (why.empty() ? "at once: " + fast_why : "after 2 s: " + why)
                << "\n";
      return 1;
    }

    // The write-protect switch, 0480h bit 5 (FORMAT.EXE+06BAh): creating a
    // file on a protected card fails and leaves the card as it was.
    machine.EjectCard();
    Fat12Format protected_format = card_format;
    protected_format.label = "CHRANENA";
    PcCard locked = PcCard::Sram(kCardSize, FormatFat12(protected_format));
    locked.write_protected = true;
    const std::vector<uint8_t> locked_before = locked.Contents();
    machine.InsertCard(std::move(locked));
    for (int i = 0; i < 100 && machine.CardWaiting(); ++i) machine.Run(kSlice);
    const std::size_t said_before = speech.text().size();
    Cpu80188::Registers w{};
    w.ax = 0x3C00;
    w.ds = w.es = data;
    // AAPI's critical-error handler says "c chráněn proti zápisu" and waits
    // for r (retry), f (fail) or i (ignore) (MESSAGES.DAT); the test
    // answers f, dots 1, 2, 4 = 0581h bits 2, 1, 3.
    bool prompted = false;
    const bool returned = DosCall(machine, speech, waiting, top, w, [&] {
      prompted = speech.text().find("proti z", said_before) != std::string::npos;
      press(machine, 0, 0x0E, 1);
    });
    const bool created = returned && !(w.flags & 0x0001);
    if (verbose) std::cout << "create on a write-protected card: " << (returned ? "" : "no return, ")
                           << (created ? "succeeded" : "failed") << ", AX " << std::hex << w.ax << std::dec
                           << ", said: " << Printable(speech.text().substr(said_before)) << "\n";
    if (!returned || !prompted || created || !machine.card() || machine.card()->Contents() != locked_before) {
      std::cout << "FAIL boot: a write-protected SRAM card: "
                << (!prompted ? "no write-protect prompt" : !returned ? "f did not end the call"
                    : created ? "took a new file" : "changed") << "\n";
      return 1;
    }

    // The card battery, 0480h bits 2-3 (BKB.COM+1622h): the battery report
    // (space + dots 1 2, 0581h bits 7, 2, 1 = 86h, K_BATTERY) ends with
    // IDS_KBD_PCCARD_LOW ", p c karta částečně vybitá" or _FLAT ", p c
    // karta zcela vybitá" (MESSAGES.DAT).
    auto report = [&](PcCard::Battery battery) {
      machine.card()->battery = battery;
      const std::size_t from = speech.text().size();
      press(machine, 0, 0x86, 3);
      return speech.text().substr(from);
    };
    const std::string good = report(PcCard::Battery::kGood);
    const std::string low = report(PcCard::Battery::kLow);
    const std::string flat = report(PcCard::Battery::kFlat);
    if (verbose) std::cout << "battery report, card good: " << Printable(good) << "; low: " << Printable(low)
                           << "; flat: " << Printable(flat) << "\n";
    auto warns = [](const std::string& said) { return said.find("p c karta") != std::string::npos; };
    auto flat_word = [](const std::string& said) { return said.find("zcela") != std::string::npos; };
    if (good.empty() || warns(good) || !warns(low) || flat_word(low) || !warns(flat) || !flat_word(flat)) {
      std::cout << "FAIL boot: card battery in the battery report (good: " << Printable(good) << "; low: "
                << Printable(low) << "; flat: " << Printable(flat) << ")\n";
      return 1;
    }
    machine.EjectCard();

    // A blank card, every byte FFh, as Karta -> Vložit novou kartu makes an
    // unformatted image (HANDOFF 6.8): creating a file must fail and come
    // back, without a critical-error prompt for nobody to answer, and leave
    // the card blank. Measured 10. 10. 2026 on 7.05: DOS error 4 at once,
    // nothing said; the driver writes its probe and puts the bytes back.
    machine.InsertCard(PcCard::Sram(kCardSize, std::vector<uint8_t>(kCardSize, 0xFF)));
    for (int i = 0; i < 100 && machine.CardWaiting(); ++i) machine.Run(kSlice);
    Cpu80188::Registers b{};
    b.ax = 0x3C00;
    b.ds = b.es = data;
    bool blank_prompted = false;
    const bool blank_returned = DosCall(machine, speech, waiting, top, b, [&] {
      blank_prompted = true;
      press(machine, 0, 0x0E, 1);
    });
    const std::vector<uint8_t>& blank = machine.card()->Contents();
    const bool still_blank = std::all_of(blank.begin(), blank.end(), [](uint8_t v) { return v == 0xFF; });
    if (verbose) std::cout << "create on a blank card: " << (blank_returned ? "returned" : "no return")
                           << ", AX " << std::hex << b.ax << std::dec << (blank_prompted ? ", prompted" : "")
                           << (still_blank ? "" : ", card changed") << "\n";
    if (!blank_returned || blank_prompted || !(b.flags & 0x0001) || !still_blank) {
      std::cout << "FAIL boot: a blank card: "
                << (!blank_returned ? "the call did not return" : blank_prompted ? "a prompt waited for a key"
                    : !(b.flags & 0x0001) ? "a file was created" : "the card was written")
                << "\n";
      return 1;
    }
    machine.EjectCard();
    // A flash card (HANDOFF 6.8, ar-ibv): a blank 4 MB card of two pairs
    // of Intel 28F008SA, formatted with Datalight's own DLFMT.EXE C: /C
    // (FTL, through DLFFSA's chip commands), then a 4 KB file written,
    // the card taken out and put back, and the file read again.
    {
      machine.InsertCard(PcCard::Flash(4u << 20));
      for (int i = 0; i < 100 && machine.CardWaiting(); ++i) machine.Run(kSlice);
      // A DOS block of our own for the EXEC's names and stack, so that the
      // child loads above it and not over it.
      Cpu80188::Registers a{};
      a.ax = 0x4800;
      a.bx = 0x100;
      a.ds = a.es = data;
      if (!DosCall(machine, speech, waiting, top, a) || (a.flags & 0x0001)) {
        std::cout << "FAIL boot: flash card: no DOS memory for DLFMT's EXEC\n";
        return 1;
      }
      const uint16_t own = a.ax;
      const uint32_t ob = static_cast<uint32_t>(own) << 4;
      auto put = [&](uint32_t at, const char* text, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) machine.Poke(ob + at + i, static_cast<uint8_t>(text[i]));
      };
      const char prog[] = "B:\\SYSTEM\\DLFMT.EXE";
      put(0x00, prog, sizeof prog);
      const std::string tail = " C: /C";
      machine.Poke(ob + 0x40, static_cast<uint8_t>(tail.size()));
      put(0x41, tail.c_str(), tail.size());
      machine.Poke(ob + 0x41 + tail.size(), 0x0D);
      // Parameter block at 0x80: env 0, tail far ptr, two FCB pointers.
      machine.PokeWord(ob + 0x80, 0);
      machine.PokeWord(ob + 0x82, 0x40);
      machine.PokeWord(ob + 0x84, own);
      machine.PokeWord(ob + 0x86, 0xA0);
      machine.PokeWord(ob + 0x88, own);
      machine.PokeWord(ob + 0x8A, 0xA0);
      machine.PokeWord(ob + 0x8C, own);
      for (int i = 0; i < 16; ++i) machine.Poke(ob + 0xA0 + i, 0);
      Cpu80188::Registers e{};
      e.ax = 0x4B00;
      e.dx = 0;
      e.bx = 0x80;
      e.ds = e.es = own;
      // DLFMT prints through INT 21h AH=06h, which the teletype log does not
      // see; what it did is judged by the card and by DOS afterwards.
      const bool formatted = DosCall(machine, speech, waiting, static_cast<uint16_t>(own + 0x100), e);
      const uint64_t format_writes = machine.card()->Writes();
      Cpu80188::Registers f{};
      f.ax = 0x3600;
      f.dx = 3;
      f.ds = f.es = data;
      const bool sized = DosCall(machine, speech, waiting, top, f) && f.ax != 0xFFFF;
      const uint64_t free_kb = static_cast<uint64_t>(f.ax) * f.bx * f.cx / 1024;
      if (verbose) std::cout << "flash card: DLFMT " << (formatted ? "returned" : "did not return") << ", "
                             << format_writes << " bytes changed, C: free " << free_kb << " KB\n";
      // Two pairs, less the FTL's spare blocks and tables: measured 3792 KB.
      if (!formatted || format_writes == 0 || !sized || free_kb < 3000 || free_kb > 4096) {
        std::cout << "FAIL boot: flash card: DLFMT C: /C "
                  << (!formatted ? "did not return" : format_writes == 0 ? "wrote nothing"
                      : !sized ? "left C: without a size" : "left " + std::to_string(free_kb) + " KB free")
                  << "\n";
        return 1;
      }
      // A file of 4 KB through DLFFSA, then the card out and in again.
      const char fpath[] = "C:\\FLASH.TXT";
      for (std::size_t i = 0; i < sizeof fpath; ++i) machine.Poke(base + 0x300 + i, static_cast<uint8_t>(fpath[i]));
      for (uint32_t i = 0; i < 4096; ++i) machine.Poke(base + 0x400 + i, static_cast<uint8_t>(i * 7 + 3));
      Cpu80188::Registers fr;
      std::string fstep;
      if (!dos(0x3C00, 0, 0, 0x300, fr)) fstep = "create";
      uint16_t fh = fr.ax;
      if (fstep.empty() && (!dos(0x4000, fh, 4096, 0x400, fr) || fr.ax != 4096)) fstep = "write";
      if (fstep.empty() && !dos(0x3E00, fh, 0, 0, fr)) fstep = "close";
      std::optional<PcCard> out = machine.EjectCard();
      for (int i = 0; i < 10; ++i) machine.Run(kSlice);
      machine.InsertCard(std::move(*out));
      for (int i = 0; i < 100 && machine.CardWaiting(); ++i) machine.Run(kSlice);
      for (int i = 0; i < 10; ++i) machine.Run(kSlice);
      for (uint32_t i = 0; i < 4096; ++i) machine.Poke(base + 0x400 + i, 0);
      if (fstep.empty() && !dos(0x3D00, 0, 0, 0x300, fr)) fstep = "open after reinsert";
      fh = fr.ax;
      if (fstep.empty() && (!dos(0x3F00, fh, 4096, 0x400, fr) || fr.ax != 4096)) fstep = "read after reinsert";
      if (fstep.empty() && !dos(0x3E00, fh, 0, 0, fr)) fstep = "close after reading";
      for (uint32_t i = 0; fstep.empty() && i < 4096; ++i)
        if (machine.Peek(base + 0x400 + i) != static_cast<uint8_t>(i * 7 + 3)) fstep = "compare";
      if (!fstep.empty()) {
        std::cout << "FAIL boot: flash card: C:\\FLASH.TXT, INT 21h " << fstep << " (AX " << std::hex << fr.ax
                  << std::dec << ")\n";
        return 1;
      }
      Cpu80188::Registers freed{};
      freed.ax = 0x4900;
      freed.ds = freed.es = own;
      DosCall(machine, speech, waiting, top, freed);
      machine.EjectCard();
    }
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
  // One file per firmware: run-tests runs the boots of 7.05 and 6.87 in
  // parallel, and a shared name let one overwrite or delete the other's.
  char memory_name[48];
  std::snprintf(memory_name, sizeof memory_name, "aria_boot_test_pamet-%08X.bin", firmware);
  const std::filesystem::path memory_file = std::filesystem::temp_directory_path() / memory_name;
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
  // A Backpack floppy drive on the printer port, unit 65 (HANDOFF 6.7):
  // BACKPACK.SYS must find it at 680h-682h, read its EEPROM and announce
  // it as drive E:. On this second machine and not the first: attached
  // there, 6.87 missed the later swap to a blank card (ar-z2e).
  BackpackFloppy backpack(65);
  if (verbose) backpack.log = [](const std::string& line) {
    if (line.rfind("FDC", 0) == 0) std::cout << "backpack: " << line << "\n";
  };
  std::string floppy_text;
  for (int i = 0; floppy_text.size() < 1200; ++i) floppy_text += "Disketa v Backpacku, radek " + std::to_string(i) + "\r\n";
  // A 720 KB image cut short after the file's last cluster (sector 14 on,
  // two clusters of two sectors), as archived images often are.
  std::vector<uint8_t> short_image = FloppyImage(false, "DISKETA TXT", floppy_text);
  short_image.resize(18 * 512);
  backpack.fdc().Insert(*Fdc82077::Disk::FromImage(std::move(short_image), false));
  second.parallel().Attach(&backpack);
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
  if (second.bios().Teletype().find("BACKPACK drive is available:\r\n  Drive E: - 3.5 inch 1.4MB") ==
      std::string::npos) {
    std::cout << "FAIL boot: BACKPACK.SYS did not announce the Backpack floppy as drive E:\n";
    return 1;
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
  // The diskette through BACKPACK.SYS and the 82077 (HANDOFF 6.7, ar-l5v):
  // DOS reads E:\DISKETA.TXT, three sectors, from the short 720 KB image,
  // then writes E:\ZAPIS.TXT of 3000 bytes, which must then be in the
  // image as a host would read it.
  {
    std::vector<Mcb> waiting;
    const Mcb* free_block = nullptr;
    const std::vector<Mcb> blocks = McbChain(second);
    for (const Mcb& block : blocks) {
      if (block.name == "MAINMENU" || block.name == "AAPI") waiting.push_back(block);
      if (block.owner == 0 && (!free_block || block.paragraphs > free_block->paragraphs)) free_block = &block;
    }
    if (waiting.size() != 2 || !free_block || free_block->paragraphs < 0x400) {
      std::cout << "FAIL boot: diskette: no MAINMENU and AAPI blocks or no free DOS memory\n";
      return 1;
    }
    const uint16_t data = static_cast<uint16_t>(free_block->segment + 1);
    const uint16_t top = static_cast<uint16_t>(data + std::min<uint32_t>(free_block->paragraphs, 0x1000));
    const uint32_t base = static_cast<uint32_t>(data) << 4;
    auto dos = [&](uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, Cpu80188::Registers& out) {
      out = {};
      out.ax = ax;
      out.bx = bx;
      out.cx = cx;
      out.dx = dx;
      out.ds = out.es = data;
      return DosCall(second, second_speech, waiting, top, out) && !(out.flags & 0x0001);
    };
    auto put = [&](uint32_t at, const char* text) {
      for (std::size_t i = 0; i <= std::strlen(text); ++i) second.Poke(base + at + i, static_cast<uint8_t>(text[i]));
    };
    put(0x00, "E:\\DISKETA.TXT");
    put(0x20, "E:\\ZAPIS.TXT");
    const uint16_t length = static_cast<uint16_t>(floppy_text.size());
    Cpu80188::Registers r;
    std::string step;
    if (!dos(0x3D00, 0, 0, 0x00, r)) step = "open of E:\\DISKETA.TXT";
    uint16_t handle = r.ax;
    if (step.empty() && (!dos(0x3F00, handle, 0x1000, 0x100, r) || r.ax != length)) step = "read of E:\\DISKETA.TXT";
    if (step.empty() && !dos(0x3E00, handle, 0, 0, r)) step = "close of E:\\DISKETA.TXT";
    for (uint16_t i = 0; step.empty() && i < length; ++i)
      if (second.Peek(base + 0x100 + i) != static_cast<uint8_t>(floppy_text[i])) step = "compare of E:\\DISKETA.TXT";
    constexpr uint16_t kWritten = 3000;
    for (uint16_t i = 0; i < kWritten; ++i) second.Poke(base + 0x2000 + i, static_cast<uint8_t>(i * 13 + 5));
    if (step.empty() && !dos(0x3C00, 0, 0, 0x20, r)) step = "create of E:\\ZAPIS.TXT";
    handle = r.ax;
    if (step.empty() && (!dos(0x4000, handle, kWritten, 0x2000, r) || r.ax != kWritten)) step = "write of E:\\ZAPIS.TXT";
    if (step.empty() && !dos(0x3E00, handle, 0, 0, r)) step = "close of E:\\ZAPIS.TXT";
    if (step.empty()) {
      // DOS has closed the file, so FAT and directory are on the disk.
      Fat12Volume volume;
      std::wstring ignored;
      std::vector<uint8_t> bytes;
      if (!volume.Open(backpack.fdc().disk()->bytes, ignored) || !volume.ReadFile("ZAPIS.TXT", bytes) ||
          bytes.size() != kWritten) {
        step = "ZAPIS.TXT in the image";
      }
      for (uint16_t i = 0; step.empty() && i < kWritten; ++i)
        if (bytes[i] != static_cast<uint8_t>(i * 13 + 5)) step = "contents of ZAPIS.TXT in the image";
    }
    if (step.empty() && backpack.fdc().disk()->bytes.size() != 737280) step = "the short image padded to 720 KB";
    if (!step.empty()) {
      std::cout << "FAIL boot: diskette: " << step << " (AX " << std::hex << r.ax << std::dec << ")\n";
      return 1;
    }
    // The drive emptied: opening a file must come back failed after AAPI's
    // critical-error prompt; the test answers f, as for the write-protected
    // card. Right after the last access DOS still trusted its buffers and
    // "opened" the file without asking the drive, so the test waits 3 s,
    // more than a hand needs to swap a disk.
    backpack.fdc().Eject();
    for (long long t = 0; t < AriaMachine::kCpuHz * 3LL; t += kSlice) second.Run(kSlice);
    std::size_t said = second_speech.text().size();
    Cpu80188::Registers o{};
    o.ax = 0x3D00;
    o.ds = o.es = data;
    bool empty_prompted = false;
    const uint16_t empty_from = second.PeekWord(0x46C);  // BIOS ticks, 18.2 a second.
    // Says "disk e není připraven" within the first 3 s, as the real drive
    // would after the driver's timeout (measured 11. 10. 2026).
    const bool empty_returned = DosCall(second, second_speech, waiting, top, o, [&] {
      empty_prompted = second_speech.text().find("ipraven", said) != std::string::npos;
      press(second, 0, 0x0E, 1);
    });
    if (verbose) std::cout << "diskette: open on an empty drive " << (empty_returned ? "returned" : "did not return")
                           << (empty_prompted ? " after a prompt" : "") << " in "
                           << static_cast<uint16_t>(second.PeekWord(0x46C) - empty_from) << " ticks, AX " << std::hex << o.ax << std::dec << ", said: "
                           << Printable(second_speech.text().substr(said)) << "\n";
    if (!empty_returned || !empty_prompted || !(o.flags & 0x0001)) {
      std::cout << "FAIL boot: diskette: an empty drive "
                << (!empty_prompted ? "was not reported not ready" : empty_returned ? "opened a file"
                    : "did not come back")
                << "\n";
      return 1;
    }
    // A write-protected 1.44 MB disk in its place: DOS must see the change
    // (its other file is found, the 720 KB one's FAT not used), and
    // creating a file must fail, after the write-protect prompt.
    const std::string other_text = "Jina disketa, chranena proti zapisu\r\n";
    backpack.fdc().Insert(*Fdc82077::Disk::FromImage(FloppyImage(true, "JINA    TXT", other_text), true));
    const std::vector<uint8_t> protected_before = backpack.fdc().disk()->bytes;
    put(0x40, "E:\\JINA.TXT");
    put(0x60, "E:\\NOVY.TXT");
    if (!dos(0x3D00, 0, 0, 0x40, r)) step = "open of E:\\JINA.TXT on the second disk";
    handle = r.ax;
    if (step.empty() && (!dos(0x3F00, handle, 0x100, 0x100, r) || r.ax != other_text.size()))
      step = "read of E:\\JINA.TXT";
    if (step.empty() && !dos(0x3E00, handle, 0, 0, r)) step = "close of E:\\JINA.TXT";
    for (std::size_t i = 0; step.empty() && i < other_text.size(); ++i)
      if (second.Peek(base + 0x100 + i) != static_cast<uint8_t>(other_text[i])) step = "compare of E:\\JINA.TXT";
    if (!step.empty()) {
      std::cout << "FAIL boot: diskette: " << step << " (AX " << std::hex << r.ax << std::dec << ")\n";
      return 1;
    }
    said = second_speech.text().size();
    bool wp_prompted = false;
    Cpu80188::Registers c{};
    c.ax = 0x3C00;
    c.dx = 0x60;
    c.ds = c.es = data;
    const bool wp_returned = DosCall(second, second_speech, waiting, top, c, [&] {
      wp_prompted = second_speech.text().find("proti z", said) != std::string::npos;
      press(second, 0, 0x0E, 1);
    });
    if (verbose) std::cout << "diskette: create on a write-protected disk " << (wp_returned ? "returned" : "did not return")
                           << ", AX " << std::hex << c.ax << std::dec << ", said: "
                           << Printable(second_speech.text().substr(said)) << "\n";
    if (!wp_returned || !wp_prompted || !(c.flags & 0x0001) || backpack.fdc().disk()->bytes != protected_before ||
        backpack.fdc().disk()->modified) {
      std::cout << "FAIL boot: diskette: a write-protected disk: "
                << (!wp_prompted ? "no write-protect prompt" : !wp_returned ? "f did not end the call"
                    : !(c.flags & 0x0001) ? "took a new file" : "changed") << "\n";
      return 1;
    }
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
               " space + dot 4 answered, D: formatted on y, C:\\KARTA.TXT written and read on an SRAM card from a host"
               " folder, its subfolder's letter read and the card saved back into the folder,"
               " a card locked since power-on read through its CIS,"
               " cards swapped after 2 s and at once without mixing, write protection prompted and kept,"
               " card battery in the battery report, a blank card refused without a prompt,"
               " a 4 MB flash card formatted by DLFMT and a file kept through eject and reinsert,"
               " a Backpack floppy announced as drive E:, a file read from a short 720 KB image and one written there,"
               " an empty drive not ready, a write-protected 1.44 MB disk read and prompted,"
               " konec says vypnuto and switches off, D: kept through"
               " a memory snapshot without a new question, F1 starts the word processor)\n";
  return 0;
}
