// Unpacks the PKLITE-compressed programs of drive B: by running their own
// decompression stub on the 80C188 core (HANDOFF 6.1, 6.12). The unpacked
// EXEs are for analysis only (tools/ports.py, ints.py, dis.py with
// ARIA_UNPACKED); the emulator itself runs the packed originals.
//
//   pklite_unpack.exe IMAGE OUTDIR   write SYSTEM\*.EXE unpacked to OUTDIR
//   pklite_unpack.exe IMAGE --check  unpack all in memory, verify, PASS
//
// IMAGE is any drive B: source LoadFlashImage accepts. OUTDIR must lie
// outside the repository: the output is third-party firmware
// (PRAVNI.md, 3.6).
//
// How: the program is loaded as DOS would load it, with a PSP and a top of
// memory of A000h. The stub first runs inside the load module, copies itself
// higher up (into the program's own allocation when that is large, so the
// address alone does not tell), continues there under a new CS,
// decompresses the original program into the load module and jumps far to
// the original entry point. That second change of CS is where the run
// stops; the first one is the move to the high copy. The few DOS services a stub uses are answered here. Running it
// twice, at two load segments 1000h apart, gives the relocation table back:
// the words that differ by exactly 1000h between the two images are the
// relocated ones.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cpu80188.h"
#include "fat12.h"
#include "flash_image.h"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kHandlers = 0xF0000;  // INT n "handler" at F000:n.
constexpr uint16_t kTopOfMemory = 0xA000;

uint16_t Le16(const std::vector<uint8_t>& data, std::size_t at) {
  return static_cast<uint16_t>(data[at] | data[at + 1] << 8);
}

struct ExeHeader {
  uint16_t last_page, pages, relocations, header_paragraphs, min_alloc, max_alloc;
  uint16_t ss, sp, ip, cs, relocation_table;
  std::size_t ModuleSize() const {
    return (pages - 1) * 512u + (last_page ? last_page : 512u) - header_paragraphs * 16u;
  }
};

bool ParseHeader(const std::vector<uint8_t>& exe, ExeHeader& h) {
  if (exe.size() < 0x1C || exe[0] != 'M' || exe[1] != 'Z') return false;
  h.last_page = Le16(exe, 2);
  h.pages = Le16(exe, 4);
  h.relocations = Le16(exe, 6);
  h.header_paragraphs = Le16(exe, 8);
  h.min_alloc = Le16(exe, 10);
  h.max_alloc = Le16(exe, 12);
  h.ss = Le16(exe, 14);
  h.sp = Le16(exe, 16);
  h.ip = Le16(exe, 20);
  h.cs = Le16(exe, 22);
  h.relocation_table = Le16(exe, 24);
  return h.header_paragraphs * 16u + h.ModuleSize() <= exe.size();
}

bool IsPklite(const std::vector<uint8_t>& exe) {
  return exe.size() > 0x40 && exe[0] == 'M' && exe[1] == 'Z' && exe[30] == 'P' && exe[31] == 'K';
}

class DosBus : public CpuBus {
 public:
  DosBus() : memory(1 << 20, 0) {}
  uint8_t ReadMemory(uint32_t address) override { return memory[address]; }
  void WriteMemory(uint32_t address, uint8_t value) override {
    memory[address] = value;
    if (track && address >= track_low && address < track_high) highest = std::max(highest, address);
  }
  uint8_t ReadIo(uint16_t) override { return 0xFF; }
  void WriteIo(uint16_t, uint8_t) override {}

  uint16_t Word(uint32_t address) const {
    return static_cast<uint16_t>(memory[address & 0xFFFFF] | memory[(address + 1) & 0xFFFFF] << 8);
  }
  void SetWord(uint32_t address, uint16_t value) {
    memory[address & 0xFFFFF] = static_cast<uint8_t>(value);
    memory[(address + 1) & 0xFFFFF] = static_cast<uint8_t>(value >> 8);
  }

  std::vector<uint8_t> memory;
  bool track = false;
  uint32_t track_low = 0, track_high = 0, highest = 0;
};

struct RunResult {
  std::vector<uint8_t> image;  // The load module after decompression.
  Cpu80188::Registers entry;   // Registers at the jump to the original entry.
};

// Answers one software interrupt the stub raised, then returns like IRET.
bool ServiceInterrupt(int number, Cpu80188& cpu, DosBus& bus, std::string& error) {
  Cpu80188::Registers r = cpu.GetRegisters();
  const uint32_t stack = (static_cast<uint32_t>(r.ss) << 4) + r.sp;
  bool carry = false;
  if (number != 0x21) {
    error = "stub raised INT " + std::to_string(number);
    return false;
  }
  const uint8_t function = static_cast<uint8_t>(r.ax >> 8);
  switch (function) {
    case 0x09:  // Print string: the stub's error messages; nothing to show.
      break;
    case 0x30:  // DOS version 5.0.
      r.ax = 0x0005;
      break;
    case 0x4A:  // Resize memory block: everything up to A000h is ours.
      break;
    case 0x25:  // Set vector.
      bus.SetWord((r.ax & 0xFF) * 4u, r.dx);
      bus.SetWord((r.ax & 0xFF) * 4u + 2, r.ds);
      break;
    case 0x35:  // Get vector.
      r.bx = bus.Word((r.ax & 0xFF) * 4u);
      r.es = bus.Word((r.ax & 0xFF) * 4u + 2);
      break;
    case 0x4C:
      error = "stub exited with code " + std::to_string(r.ax & 0xFF);
      return false;
    default: {
      char text[64];
      std::snprintf(text, sizeof text, "stub called INT 21h AH=%02Xh", function);
      error = text;
      return false;
    }
  }
  // IRET by hand, with CF of the saved flags as the DOS result.
  r.ip = bus.Word(stack);
  r.cs = bus.Word(stack + 2);
  r.flags = static_cast<uint16_t>((bus.Word(stack + 4) & ~1u) | (carry ? 1u : 0u));
  r.sp = static_cast<uint16_t>(r.sp + 6);
  cpu.SetRegisters(r);
  return true;
}

bool RunStub(const std::vector<uint8_t>& exe, uint16_t psp, RunResult& result, std::string& error) {
  ExeHeader h;
  if (!ParseHeader(exe, h)) {
    error = "not an EXE";
    return false;
  }
  DosBus bus;
  const uint16_t load = static_cast<uint16_t>(psp + 0x10);
  const uint32_t load_address = static_cast<uint32_t>(load) << 4;
  const std::size_t module = h.ModuleSize();
  std::copy_n(exe.begin() + h.header_paragraphs * 16, module, bus.memory.begin() + load_address);
  for (uint16_t i = 0; i < h.relocations; ++i) {
    const std::size_t entry = h.relocation_table + i * 4u;
    const uint32_t where = load_address + (static_cast<uint32_t>(Le16(exe, entry + 2)) << 4) + Le16(exe, entry);
    bus.SetWord(where, static_cast<uint16_t>(bus.Word(where) + load));
  }
  // PSP: INT 20h at 0, top of memory at 2, empty command tail at 80h.
  const uint32_t psp_address = static_cast<uint32_t>(psp) << 4;
  bus.memory[psp_address] = 0xCD;
  bus.memory[psp_address + 1] = 0x20;
  bus.SetWord(psp_address + 2, kTopOfMemory);
  bus.memory[psp_address + 0x81] = 0x0D;
  for (int n = 0; n < 256; ++n) {
    bus.SetWord(n * 4u, static_cast<uint16_t>(n));
    bus.SetWord(n * 4u + 2, 0xF000);
    bus.memory[kHandlers + n] = 0xCF;  // IRET, never reached: serviced above.
  }

  Cpu80188 cpu(bus);
  Cpu80188::Registers r;
  r.cs = static_cast<uint16_t>(h.cs + load);
  r.ip = h.ip;
  r.ss = static_cast<uint16_t>(h.ss + load);
  r.sp = h.sp;
  r.ds = r.es = psp;
  r.flags = 0xF202;
  cpu.SetRegisters(r);

  // The decompressed program may need more than the packed module: the
  // header's minimum allocation is what the stub asks DOS for.
  const uint32_t region_end = load_address + static_cast<uint32_t>(module) + h.min_alloc * 16u;
  bus.track_low = load_address;
  bus.track_high = region_end;
  const uint16_t first_cs = r.cs;
  uint16_t stub_cs = 0;
  bool moved = false;
  for (long step = 0; step < 200'000'000; ++step) {
    const Cpu80188::Registers now = cpu.GetRegisters();
    const uint32_t pc = (static_cast<uint32_t>(now.cs) << 4) + now.ip;
    if (pc >= kHandlers && pc < kHandlers + 256) {
      if (!ServiceInterrupt(static_cast<int>(pc - kHandlers), cpu, bus, error)) return false;
      continue;
    }
    if (!moved && now.cs != first_cs) {
      moved = true;  // The stub runs from its high copy now.
      stub_cs = now.cs;
      bus.track = true;
      bus.highest = load_address;
      // Its own copy lies above the decompressed program; writes there
      // are not part of the image.
      bus.track_high = std::min(region_end, static_cast<uint32_t>(stub_cs) << 4);
    } else if (moved && now.cs != stub_cs) {
      result.entry = now;
      result.image.assign(bus.memory.begin() + load_address, bus.memory.begin() + bus.highest + 1);
      return true;
    }
    if (cpu.Halted()) {
      error = "CPU halted in the stub";
      return false;
    }
    cpu.Step();
  }
  error = "stub did not finish";
  return false;
}

struct Unpacked {
  std::vector<uint8_t> exe;
  std::size_t relocations = 0;
  std::size_t image_size = 0;
};

bool Unpack(const std::vector<uint8_t>& packed, Unpacked& out, std::string& error) {
  constexpr uint16_t kPspA = 0x1000, kPspB = 0x2000, kDelta = kPspB - kPspA;
  RunResult a, b;
  if (!RunStub(packed, kPspA, a, error) || !RunStub(packed, kPspB, b, error)) return false;
  const std::size_t size = std::min(a.image.size(), b.image.size());
  std::vector<uint8_t> image(a.image.begin(), a.image.begin() + static_cast<std::ptrdiff_t>(size));
  std::vector<uint32_t> relocations;
  // Adding 1000h never changes the low byte of a word, so a relocated
  // word at offset i shows up as a difference at byte i + 1 only.
  for (std::size_t j = 1; j < size; ++j) {
    if (a.image[j] == b.image[j]) continue;
    const std::size_t i = j - 1;
    const uint16_t wa = static_cast<uint16_t>(a.image[i] | a.image[j] << 8);
    const uint16_t wb = static_cast<uint16_t>(b.image[i] | b.image[j] << 8);
    if (static_cast<uint16_t>(wb - wa) != kDelta) {
      char text[160];
      std::snprintf(text, sizeof text,
                    "images differ at %05zXh (%04X vs %04X) in a way that is not a relocation; "
                    "stopped at %04X:%04X / %04X:%04X, image %zu B",
                    i, wa, wb, a.entry.cs, a.entry.ip, b.entry.cs, b.entry.ip, size);
      error = text;
      return false;
    }
    const uint16_t original = static_cast<uint16_t>(wa - (kPspA + 0x10));
    image[i] = static_cast<uint8_t>(original);
    image[i + 1] = static_cast<uint8_t>(original >> 8);
    relocations.push_back(static_cast<uint32_t>(i));
  }
  if (size > 0 && a.image[0] != b.image[0]) {
    error = "images differ at byte 0, which cannot be a relocated word";
    return false;
  }
  const uint16_t load_a = kPspA + 0x10;
  if (static_cast<uint16_t>(b.entry.cs - a.entry.cs) != kDelta ||
      static_cast<uint16_t>(b.entry.ss - a.entry.ss) != kDelta) {
    error = "entry CS/SS do not move with the load segment";
    return false;
  }

  // A plain MZ header for the unpacked module.
  const std::size_t header_bytes = (0x1C + relocations.size() * 4 + 15) / 16 * 16;
  std::vector<uint8_t> exe(header_bytes, 0);
  auto put = [&](std::size_t at, std::size_t value) {
    exe[at] = static_cast<uint8_t>(value);
    exe[at + 1] = static_cast<uint8_t>(value >> 8);
  };
  const std::size_t total = header_bytes + image.size();
  const uint16_t ss = static_cast<uint16_t>(a.entry.ss - load_a);
  // Room for the stack if it lies beyond the image, as the minimum
  // allocation the program needs at least.
  const std::size_t stack_top = (static_cast<std::size_t>(ss) << 4) + a.entry.sp;
  const std::size_t min_alloc = stack_top > image.size() ? (stack_top - image.size() + 15) / 16 : 0;
  exe[0] = 'M';
  exe[1] = 'Z';
  put(2, total % 512);
  put(4, (total + 511) / 512);
  put(6, relocations.size());
  put(8, header_bytes / 16);
  put(10, min_alloc);
  put(12, 0xFFFF);
  put(14, ss);
  put(16, a.entry.sp);
  put(20, a.entry.ip);
  put(22, static_cast<uint16_t>(a.entry.cs - load_a));
  put(24, 0x1C);
  for (std::size_t n = 0; n < relocations.size(); ++n) {
    put(0x1C + n * 4, relocations[n] & 0xF);
    put(0x1C + n * 4 + 2, relocations[n] >> 4);
  }
  exe.insert(exe.end(), image.begin(), image.end());
  out.exe = std::move(exe);
  out.relocations = relocations.size();
  out.image_size = image.size();
  return true;
}

bool Contains(const std::vector<uint8_t>& data, const char* text) {
  const std::size_t length = std::strlen(text);
  return std::search(data.begin(), data.end(), text, text + length) != data.end();
}

// Win32 rather than std::filesystem: fs::weakly_canonical throws and
// fs::absolute drops a backslash on the UNC paths a tree inside WSL has
// from Windows (\\wsl.localhost\...), measured 6. 10. 2026.
fs::path Normalized(const fs::path& path) {
  wchar_t full[4096];
  const DWORD length = GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(full)), full, nullptr);
  if (length == 0 || length >= std::size(full)) return path.lexically_normal();
  return fs::path(full).lexically_normal();
}

fs::path RepositoryRoot() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return Normalized(fs::path(path)).parent_path().parent_path();  // bin\..
}

bool IsInside(const fs::path& inner, const fs::path& outer) {
  auto i = inner.begin();
  for (auto o = outer.begin(); o != outer.end(); ++o, ++i) {
    if (i == inner.end() || _wcsicmp(i->c_str(), o->c_str()) != 0) return false;
  }
  return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 3) {
    std::cout << "usage: pklite_unpack IMAGE OUTDIR | IMAGE --check\n";
    return 2;
  }
  FlashImage flash;
  std::wstring load_error;
  if (!LoadFlashImage(argv[1], TextEncoding::kUnknown, flash, load_error)) {
    std::cout << "SKIP pklite_unpack: cannot load the drive B: image\n";
    return 2;
  }
  const bool check = std::wstring(argv[2]) == L"--check";
  fs::path out_dir;
  if (!check) {
    out_dir = Normalized(fs::path(argv[2]));
    if (IsInside(out_dir, RepositoryRoot())) {
      std::wcout << L"refusing to write unpacked firmware into the repository: " << out_dir.wstring() << L"\n";
      return 1;
    }
    std::error_code error;
    fs::create_directories(out_dir, error);
    if (error) {
      std::wcout << L"cannot create " << out_dir.wstring() << L"\n";
      return 1;
    }
  }

  Fat12Volume volume;
  std::wstring fat_error;
  volume.Open(flash.bytes, fat_error);
  std::vector<FatEntry> entries;
  volume.List("SYSTEM", entries);
  int packed_count = 0, failures = 0;
  for (const FatEntry& entry : entries) {
    std::vector<uint8_t> data;
    if (!volume.ReadFile("SYSTEM\\" + entry.name, data) || !IsPklite(data)) continue;
    ++packed_count;
    Unpacked unpacked;
    std::string error;
    if (!Unpack(data, unpacked, error)) {
      std::cout << "FAIL " << entry.name << ": " << error << "\n";
      ++failures;
      continue;
    }
    // A sound result has relocations, no trace of the stub's copyright
    // text, and the copyright of the compiler runtime the program was
    // built with: Borland for Robotron's programs, Datalight's own for
    // DLFMT.EXE (measured on 7.05).
    const bool runtime = Contains(unpacked.exe, "Borland") || Contains(unpacked.exe, "Datalight");
    const bool stub_left = Contains(unpacked.exe, "PKLITE Copr.");
    std::cout << (runtime && !stub_left && unpacked.relocations > 0 ? "ok   " : "FAIL ") << entry.name
              << ": image " << unpacked.image_size << " B, " << unpacked.relocations << " relocations"
              << (runtime ? "" : ", no known runtime copyright") << (stub_left ? ", stub text left" : "") << "\n";
    if (!runtime || stub_left || unpacked.relocations == 0) ++failures;
    if (!check) {
      std::ofstream file(out_dir / entry.name, std::ios::binary);
      file.write(reinterpret_cast<const char*>(unpacked.exe.data()), static_cast<std::streamsize>(unpacked.exe.size()));
    }
  }
  const bool passed = failures == 0 && packed_count > 0;
  std::cout << (passed ? "PASS" : "FAIL") << " pklite_unpack (" << packed_count << " programs)\n";
  return passed ? 0 : 1;
}
