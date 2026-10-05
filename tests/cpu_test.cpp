// Instruction tests of the 80C188 core (src/cpu80188.*) against the
// hardware-generated SingleStepTests suites (HANDOFF 6.12):
//
//   cpu_test.exe CPUTESTS 8088 [FILTER [-v]]   8088 v2, MOO binary format
//   cpu_test.exe CPUTESTS v20  [FILTER [-v]]   80186 extensions, V20 set
//
// CPUTESTS is the folder from ARIA_CPUTESTS (D:\aria\cputests), not in the
// repository. FILTER limits the run to files whose name starts with it;
// -v prints the full state and every difference of the first failures.
//
// The Aria has an 80188, and neither suite was recorded on one, so the
// policy below decides what is compared. Everything that is not compared
// is counted and printed, never dropped silently:
//   - metadata "normal": compared exactly, minus the undefined flags given
//     by the metadata flags-mask;
//   - metadata "undocumented", and "alias" except where the 80186 gives
//     the opcode another meaning: compared, reported apart, not failing;
//   - "alias" of 60h-6Fh, C0h, C1h, C8h, C9h (80186 instructions there),
//     "undefined", "prefix", "fpu": skipped;
//   - known 80186 differences, see kKnown186Differences below.
// Bus cycles and the prefetch queue are not compared: the emulator is not
// cycle-exact (HANDOFF 6.12).
//
// Prints PASS on success; exit code 2 when the test data is missing.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "cpu80188.h"
#include "json_lite.h"
#include "zip_reader.h"

namespace fs = std::filesystem;

namespace {

using Registers = Cpu80188::Registers;

constexpr const char* kRegisterNames[14] = {"ax", "bx", "cx", "dx", "cs", "ss", "ds",
                                            "es", "sp", "bp", "si", "di", "ip", "flags"};

uint16_t& RegisterAt(Registers& registers, int index) {
  uint16_t* fields[14] = {&registers.ax, &registers.bx, &registers.cx, &registers.dx,
                          &registers.cs, &registers.ss, &registers.ds, &registers.es,
                          &registers.sp, &registers.bp, &registers.si, &registers.di,
                          &registers.ip, &registers.flags};
  return *fields[index];
}

struct TestCase {
  std::string name;
  std::vector<uint8_t> bytes;
  Registers initial;
  Registers final;
  uint16_t final_mask = 0;  // Bit i: register i given in the final state.
  std::vector<std::pair<uint32_t, uint8_t>> ram_initial;
  std::vector<std::pair<uint32_t, uint8_t>> ram_final;
};

// 1 MB of RAM as the tests assume; only the bytes a test touched are reset.
class TestBus : public CpuBus {
 public:
  TestBus() : memory_(1 << 20, 0) {}

  void Load(const TestCase& test) {
    for (uint32_t address : touched_) memory_[address] = 0;
    touched_.clear();
    for (const auto& [address, value] : test.ram_initial) Poke(address, value);
  }

  uint8_t Peek(uint32_t address) const { return memory_[address & 0xFFFFF]; }

  uint8_t ReadMemory(uint32_t address) override { return memory_[address]; }
  void WriteMemory(uint32_t address, uint8_t value) override { Poke(address, value); }
  // The test rig answers I/O reads with an open bus.
  uint8_t ReadIo(uint16_t) override { return 0xFF; }
  void WriteIo(uint16_t, uint8_t) override {}
  uint8_t AcknowledgeInterrupt() override { return 0; }

 private:
  void Poke(uint32_t address, uint8_t value) {
    address &= 0xFFFFF;
    memory_[address] = value;
    touched_.push_back(address);
  }

  std::vector<uint8_t> memory_;
  std::vector<uint32_t> touched_;
};

bool ReadFile(const fs::path& path, std::vector<uint8_t>& data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return true;
}

uint32_t Le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
uint16_t Le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }

// MOO state chunk (INIT or FINA): REGS, RAM , QUEU subchunks.
void ParseMooState(const uint8_t* data, std::size_t length, Registers& registers, uint16_t& mask,
                   std::vector<std::pair<uint32_t, uint8_t>>& ram) {
  std::size_t at = 0;
  while (at + 8 <= length) {
    const std::string type(reinterpret_cast<const char*>(data + at), 4);
    const uint32_t size = Le32(data + at + 4);
    const uint8_t* body = data + at + 8;
    if (type == "REGS") {
      mask = Le16(body);
      std::size_t offset = 2;
      for (int index = 0; index < 14; ++index) {
        if (mask & (1u << index)) {
          RegisterAt(registers, index) = Le16(body + offset);
          offset += 2;
        }
      }
    } else if (type == "RAM ") {
      const uint32_t count = Le32(body);
      for (uint32_t entry = 0; entry < count; ++entry) {
        ram.emplace_back(Le32(body + 4 + entry * 5), body[4 + entry * 5 + 4]);
      }
    }
    at += 8 + size;
  }
}

bool ParseMoo(const std::vector<uint8_t>& file, std::vector<TestCase>& tests) {
  std::size_t at = 0;
  while (at + 8 <= file.size()) {
    const std::string type(reinterpret_cast<const char*>(&file[at]), 4);
    const uint32_t size = Le32(&file[at + 4]);
    if (at + 8 + size > file.size()) return false;
    if (type == "TEST") {
      TestCase test;
      const uint8_t* body = &file[at + 8];
      std::size_t inner = 4;  // Skip the index.
      while (inner + 8 <= size) {
        const std::string sub(reinterpret_cast<const char*>(body + inner), 4);
        const uint32_t sub_size = Le32(body + inner + 4);
        const uint8_t* data = body + inner + 8;
        if (sub == "NAME") {
          test.name.assign(reinterpret_cast<const char*>(data + 4), Le32(data));
        } else if (sub == "BYTS") {
          test.bytes.assign(data + 4, data + 4 + Le32(data));
        } else if (sub == "INIT") {
          uint16_t ignored = 0;
          ParseMooState(data, sub_size, test.initial, ignored, test.ram_initial);
        } else if (sub == "FINA") {
          ParseMooState(data, sub_size, test.final, test.final_mask, test.ram_final);
        }
        inner += 8 + sub_size;
      }
      tests.push_back(std::move(test));
    }
    at += 8 + size;
  }
  return true;
}

void JsonState(const JsonValue& state, Registers& registers, uint16_t* mask,
               std::vector<std::pair<uint32_t, uint8_t>>& ram) {
  if (const JsonValue* regs = state.Find("regs")) {
    for (int index = 0; index < 14; ++index) {
      if (const JsonValue* value = regs->Find(kRegisterNames[index])) {
        RegisterAt(registers, index) = static_cast<uint16_t>(value->number);
        if (mask) *mask = static_cast<uint16_t>(*mask | 1u << index);
      }
    }
  }
  if (const JsonValue* entries = state.Find("ram")) {
    for (const JsonValue& entry : entries->items) {
      ram.emplace_back(static_cast<uint32_t>(entry.items[0].number), static_cast<uint8_t>(entry.items[1].number));
    }
  }
}

bool ParseJsonTests(const std::vector<uint8_t>& file, std::vector<TestCase>& tests) {
  JsonValue root;
  JsonReader reader(reinterpret_cast<const char*>(file.data()), file.size());
  if (!reader.Parse(root) || root.type != JsonValue::Type::kArray) return false;
  for (const JsonValue& item : root.items) {
    TestCase test;
    if (const JsonValue* name = item.Find("name")) test.name = name->text;
    if (const JsonValue* bytes = item.Find("bytes")) {
      for (const JsonValue& byte : bytes->items) test.bytes.push_back(static_cast<uint8_t>(byte.number));
    }
    if (const JsonValue* initial = item.Find("initial")) JsonState(*initial, test.initial, nullptr, test.ram_initial);
    if (const JsonValue* final = item.Find("final")) JsonState(*final, test.final, &test.final_mask, test.ram_final);
    tests.push_back(std::move(test));
  }
  return true;
}

// Metadata entry for a file name "F6.3.MOO.gz" / "C0.4.json.gz" / "27.MOO.gz".
struct Policy {
  std::string status = "normal";
  uint16_t flags_mask = 0xFFFF;
};

Policy PolicyFor(const JsonValue& metadata, const std::string& opcode, const std::string& reg) {
  Policy policy;
  const JsonValue* opcodes = metadata.Find("opcodes");
  const JsonValue* entry = opcodes ? opcodes->Find(opcode) : nullptr;
  if (entry && !reg.empty()) {
    if (const JsonValue* regs = entry->Find("reg")) entry = regs->Find(reg);
  }
  if (!entry) return policy;
  if (const JsonValue* status = entry->Find("status")) policy.status = status->text;
  if (const JsonValue* mask = entry->Find("flags-mask")) policy.flags_mask = static_cast<uint16_t>(mask->number);
  return policy;
}

// Where the 80186 knowingly differs from the 8088 in an opcode the 8088
// metadata calls normal. Each entry is a documented 80186 behaviour, seen
// in MAME's i186.cpp (HANDOFF 6.12).
struct KnownDifference {
  const char* opcode;
  const char* reason;
};
constexpr KnownDifference kKnown186Differences[] = {
    {"0F", "8088 POP CS; on the 80186 an invalid opcode (INT 6), i186.cpp 669"},
};

// Per-test exclusions inside otherwise compared opcodes.
const char* SkipReason(const std::string& opcode, const TestCase& test, bool v20) {
  // The 80186 masks shift counts to 5 bits, the 8088 does not
  // (i186.cpp 488, 513). Counts below 32 behave the same.
  if (!v20 && (opcode == "D2" || opcode == "D3") && (test.initial.cx & 0xFF) >= 32) {
    return "shift count >= 32 (80186 masks to 5 bits)";
  }
  // After reset the 80186 decodes I/O FF00h-FFFFh as its own peripheral
  // control block (relocation register 20FFh, i186.cpp device_reset), so
  // these ports are not external as on the 8088. The Aria moves the PCB to
  // 0000h (HANDOFF 6.1); either way the 8088 result does not apply.
  if (!v20 && (opcode == "EC" || opcode == "ED" || opcode == "EE" || opcode == "EF") && test.initial.dx >= 0xFF00) {
    return "port FF00h-FFFFh is the 80186 PCB after reset";
  }
  // MOV Sreg with register field 1 (CS) or 4-7: the 8088 loads CS or the
  // alias reg&3, MAME's 80186 raises INT 6 (i186.cpp 399). Not verified on
  // an 80186; no assembler emits these encodings.
  if (!v20 && opcode == "8E") {
    std::size_t at = 0;
    while (at < test.bytes.size() && test.bytes[at] != 0x8E) ++at;
    if (at + 1 < test.bytes.size()) {
      const int reg = (test.bytes[at + 1] >> 3) & 7;
      if (reg == 1 || reg >= 4) return "MOV CS / Sreg alias 4-7 (80186: INT 6 in MAME)";
    }
  }
  // A REP prefix in front of IDIV makes the 8088 microcode negate the
  // quotient, an artefact of its microcode; nothing suggests the 80186
  // shares it, and no compiler emits it.
  if (!v20 && (opcode == "F6" || opcode == "F7") && !test.bytes.empty()) {
    const uint8_t* byte = test.bytes.data();
    const uint8_t* end = byte + test.bytes.size();
    bool rep = false;
    for (; byte < end && (*byte == 0x26 || *byte == 0x2E || *byte == 0x36 || *byte == 0x3E || *byte == 0xF2 ||
                          *byte == 0xF3);
         ++byte) {
      rep = rep || *byte == 0xF2 || *byte == 0xF3;
    }
    if (rep && byte + 1 < end && ((byte[1] >> 3) & 7) == 7) return "REP before IDIV (8088 microcode quirk)";
  }
  // The V20 does not mask shift counts, the 80186 masks them to 5 bits
  // (i186.cpp 409, 434; MAME i86.txt "NEC: shift count not anded").
  if (v20 && (opcode == "C0" || opcode == "C1") && !test.bytes.empty() && test.bytes.back() >= 32) {
    return "shift count >= 32 (V20 does not mask, 80186 does)";
  }
  // INS/OUTS on ports FF00h-FFFFh hit the 80186 PCB after reset, as for
  // IN/OUT in the 8088 set above.
  if (v20 && (opcode == "6C" || opcode == "6D" || opcode == "6E" || opcode == "6F") && test.initial.dx >= 0xFF00) {
    return "port FF00h-FFFFh is the 80186 PCB after reset";
  }
  // REPC/REPNC (64h/65h) exist only on the V20; on the 80186 they are
  // invalid opcodes.
  if (v20 && !test.bytes.empty()) {
    for (uint8_t byte : test.bytes) {
      if (byte == 0x64 || byte == 0x65) return "V20-only REPC/REPNC prefix";
      if (byte != 0x26 && byte != 0x2E && byte != 0x36 && byte != 0x3E && byte != 0xF2 && byte != 0xF3) break;
    }
  }
  return nullptr;
}

std::string Hex(uint32_t value, int digits) {
  char buffer[16];
  std::snprintf(buffer, sizeof buffer, "%0*X", digits, value);
  return buffer;
}

// A divide error (DIV/IDIV overflow, AAM 0) traps through INT 0 with
// whatever flags the division microcode left behind. Intel leaves the
// arithmetic flags undefined there, and the 80186 divides with other
// microcode than the 8088, so they are not compared: neither in FLAGS nor
// in the copy pushed on the stack. Recognised by the expected final CS:IP
// being vector 0 and SP having dropped by the 6 bytes of the frame.
constexpr uint16_t kArithmeticFlags = 0x08D5;  // OF SF ZF AF PF CF

uint16_t ExpectedWordAt(const TestCase& test, uint32_t address) {
  uint16_t value = 0;
  for (const auto& [a, v] : test.ram_initial) {
    if (a == address) value = static_cast<uint16_t>((value & 0xFF00) | v);
    if (a == ((address + 1) & 0xFFFFF)) value = static_cast<uint16_t>((value & 0x00FF) | v << 8);
  }
  return value;
}

bool IsDivideErrorTrap(const TestCase& test, const Registers& expected) {
  return expected.ip == ExpectedWordAt(test, 0) && expected.cs == ExpectedWordAt(test, 2) &&
         static_cast<uint16_t>(test.initial.sp - 6) == expected.sp;
}

// Returns an empty string on a match, else a description of the first
// difference, or of all of them when `all` is set.
std::string Compare(const TestCase& test, const Cpu80188& cpu, const TestBus& bus, uint16_t flags_mask,
                    bool divide_error_possible, bool all = false) {
  std::string report;
  Registers expected = test.initial;
  for (int index = 0; index < 14; ++index) {
    if (test.final_mask & (1u << index)) RegisterAt(expected, index) = RegisterAt(const_cast<Registers&>(test.final), index);
  }
  Registers actual = cpu.GetRegisters();
  // Stack address of the pushed FLAGS when this test traps with INT 0.
  uint32_t pushed_flags = 0xFFFFFFFF;
  if (divide_error_possible && IsDivideErrorTrap(test, expected)) {
    flags_mask = static_cast<uint16_t>(flags_mask & ~kArithmeticFlags);
    pushed_flags = (static_cast<uint32_t>(expected.ss) << 4) + static_cast<uint16_t>(expected.sp + 4);
  }
  for (int index = 0; index < 14; ++index) {
    uint16_t want = RegisterAt(expected, index);
    uint16_t got = RegisterAt(actual, index);
    if (index == 13) {
      want &= flags_mask;
      got &= flags_mask;
    }
    if (want != got) {
      report += (report.empty() ? "" : "; ") + std::string(kRegisterNames[index]) + " expected " + Hex(want, 4) +
                " got " + Hex(got, 4);
      if (!all) return report;
    }
  }
  std::map<uint32_t, uint8_t> memory;
  for (const auto& [address, value] : test.ram_initial) memory[address] = value;
  for (const auto& [address, value] : test.ram_final) memory[address] = value;
  for (const auto& [address, value] : memory) {
    uint8_t byte_mask = 0xFF;
    if (pushed_flags != 0xFFFFFFFF) {
      const uint32_t high = (static_cast<uint32_t>(expected.ss) << 4) + static_cast<uint16_t>(expected.sp + 5);
      if (address == (pushed_flags & 0xFFFFF)) byte_mask = static_cast<uint8_t>(flags_mask);
      if (address == (high & 0xFFFFF)) byte_mask = static_cast<uint8_t>(flags_mask >> 8);
    }
    if ((bus.Peek(address) & byte_mask) != (value & byte_mask)) {
      report += (report.empty() ? "" : "; ") + std::string("ram[") + Hex(address, 5) + "] expected " +
                Hex(value, 2) + " got " + Hex(bus.Peek(address), 2);
      if (!all) return report;
    }
  }
  return report;
}

std::string Describe(const TestCase& test) {
  std::string text = test.name + " [";
  for (uint8_t byte : test.bytes) text += Hex(byte, 2) + " ";
  const Registers& r = test.initial;
  text += "] ax=" + Hex(r.ax, 4) + " bx=" + Hex(r.bx, 4) + " cx=" + Hex(r.cx, 4) + " dx=" + Hex(r.dx, 4) +
          " cs=" + Hex(r.cs, 4) + " ss=" + Hex(r.ss, 4) + " ds=" + Hex(r.ds, 4) + " es=" + Hex(r.es, 4) +
          " sp=" + Hex(r.sp, 4) + " bp=" + Hex(r.bp, 4) + " si=" + Hex(r.si, 4) + " di=" + Hex(r.di, 4) +
          " ip=" + Hex(r.ip, 4) + " flags=" + Hex(r.flags, 4);
  return text;
}

struct Totals {
  long compared = 0, failed = 0, undocumented = 0, undocumented_failed = 0, skipped_tests = 0;
  int skipped_files = 0;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 3) {
    std::cout << "usage: cpu_test CPUTESTS 8088|v20 [FILTER]\n";
    return 2;
  }
  const fs::path root(argv[1]);
  const std::wstring suite = argv[2];
  const bool v20 = suite == L"v20";
  std::string filter;
  if (argc > 3) {
    for (const wchar_t* p = argv[3]; *p; ++p) filter.push_back(static_cast<char>(*p));
  }
  // A fourth argument "-v" prints every difference of the first failures.
  const bool verbose = argc > 4 && std::wstring(argv[4]) == L"-v";
  const fs::path folder = v20 ? root / "v20" / "v1_native" : root / "8088" / "v2_binary";
  const fs::path metadata_path = v20 ? root / "v20" / "metadata.json" : root / "8088" / "metadata.json";
  std::error_code ignored;
  if (!fs::is_directory(folder, ignored) || !fs::exists(metadata_path, ignored)) {
    std::cout << "SKIP cpu_test: missing " << folder.string() << " or metadata.json\n";
    return 2;
  }
  std::vector<uint8_t> raw;
  JsonValue metadata;
  ReadFile(metadata_path, raw);
  if (!JsonReader(reinterpret_cast<const char*>(raw.data()), raw.size()).Parse(metadata)) {
    std::cout << "FAIL cpu_test: cannot parse metadata.json\n";
    return 1;
  }

  std::vector<fs::path> files;
  for (const auto& entry : fs::directory_iterator(folder)) {
    const std::string name = entry.path().filename().string();
    if (name.size() > 3 && name.compare(name.size() - 3, 3, ".gz") == 0 && name.rfind(filter, 0) == 0) {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());

  TestBus bus;
  Cpu80188 cpu(bus);
  Totals totals;
  std::vector<uint8_t> unpacked;
  for (const fs::path& path : files) {
    const std::string name = path.filename().string();
    const std::string opcode = name.substr(0, 2);
    const std::string reg = name.size() > 3 && name[2] == '.' && name[3] >= '0' && name[3] <= '7' &&
                                    name[4] == '.'
                                ? std::string(1, name[3])
                                : std::string();
    const Policy policy = PolicyFor(metadata, opcode, reg);
    const char* known = nullptr;
    if (!v20) {
      for (const auto& difference : kKnown186Differences) {
        if (opcode == difference.opcode) known = difference.reason;
      }
    }
    // 8088 aliases of opcodes the 80186 redefines are skipped; the other
    // aliases (82h = 80h, F6h /1 = /0, ...) are compared for information.
    static const char* const kRedefinedBy186[] = {"60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
                                                  "6A", "6B", "6C", "6D", "6E", "6F", "C0", "C1", "C8", "C9"};
    const bool redefined = !v20 && std::any_of(std::begin(kRedefinedBy186), std::end(kRedefinedBy186),
                                               [&](const char* code) { return opcode == code; });
    const bool informational = policy.status == "undocumented" || (policy.status == "alias" && !redefined);
    if (known || (policy.status != "normal" && !informational)) {
      std::cout << "skip " << name << ": " << (known ? known : ("metadata status " + policy.status).c_str()) << "\n";
      ++totals.skipped_files;
      continue;
    }

    if (!ReadFile(path, raw) || !Gunzip(raw, unpacked)) {
      std::cout << "FAIL cannot unpack " << name << "\n";
      ++totals.failed;
      continue;
    }
    std::vector<TestCase> tests;
    if (!(v20 ? ParseJsonTests(unpacked, tests) : ParseMoo(unpacked, tests))) {
      std::cout << "FAIL cannot parse " << name << "\n";
      ++totals.failed;
      continue;
    }

    const bool undocumented = informational;  // Reported, not failing.
    // Opcodes that can raise a divide error (see IsDivideErrorTrap).
    const bool divides = opcode == "D4" || ((opcode == "F6" || opcode == "F7") && (reg == "6" || reg == "7"));
    long failed = 0, compared = 0, skipped = 0;
    std::string first_failure;
    std::map<std::string, long> skip_reasons;
    for (const TestCase& test : tests) {
      if (const char* reason = SkipReason(opcode, test, v20)) {
        ++skipped;
        ++skip_reasons[reason];
        continue;
      }
      bus.Load(test);
      cpu.Reset();
      cpu.SetRegisters(test.initial);
      cpu.Step();
      ++compared;
      const std::string difference = Compare(test, cpu, bus, policy.flags_mask, divides);
      if (!difference.empty()) {
        if (failed == 0) first_failure = test.name + ": " + difference;
        if (verbose && failed < 4) {
          std::cout << "  " << Describe(test) << "\n    -> " << Compare(test, cpu, bus, policy.flags_mask, divides, true)
                    << "\n";
        }
        ++failed;
      }
    }
    if (undocumented) {
      totals.undocumented += compared;
      totals.undocumented_failed += failed;
    } else {
      totals.compared += compared;
      totals.failed += failed;
    }
    totals.skipped_tests += skipped;
    if (failed || skipped) {
      std::cout << (failed ? (undocumented ? "info " : "FAIL ") : "ok   ") << name << ": " << failed << "/" << compared
                << " failed";
      for (const auto& [reason, count] : skip_reasons) std::cout << ", " << count << " skipped (" << reason << ")";
      if (failed) std::cout << "\n     first: " << first_failure;
      std::cout << (undocumented ? " [undocumented, not counted]" : "") << "\n";
    }
  }

  std::cout << "compared " << totals.compared << ", failed " << totals.failed << "; undocumented compared "
            << totals.undocumented << ", failed " << totals.undocumented_failed << "; skipped tests "
            << totals.skipped_tests << ", skipped files " << totals.skipped_files << "\n";
  const bool passed = totals.failed == 0 && totals.compared > 0;
  std::cout << (passed ? "PASS" : "FAIL") << " cpu_test " << (v20 ? "v20" : "8088") << "\n";
  return passed ? 0 : 1;
}
