// The ADSP-2105 facade (src/adsp2105.*) without firmware: boot page
// loading in the byte order of the ADSP-2100 Family User's Manual (10.4.4)
// and a short hand-encoded program run on MAME's core. Prints PASS on
// success.

#include <algorithm>
#include <iostream>
#include <vector>

#include "adsp2105.h"

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL " << what << "\n";
    ++failures;
  }
}

class Board : public Adsp2105Board {
 public:
  void Log(const char* text) override { std::cout << "  DSP log: " << text; }
};

// A boot page for `words` (padded to a multiple of 8): the length byte,
// then the words from the last to word 0, each upper, lower, middle byte.
std::vector<uint8_t> BootPage(std::vector<uint32_t> words) {
  while (words.size() % 8) words.push_back(0);
  std::vector<uint8_t> page = {static_cast<uint8_t>(words.size() / 8 - 1)};
  for (std::size_t k = words.size(); k-- > 0;) {
    page.push_back(static_cast<uint8_t>(words[k] >> 16));
    page.push_back(static_cast<uint8_t>(words[k]));
    page.push_back(static_cast<uint8_t>(words[k] >> 8));
  }
  return page;
}

void TestBootOrder() {
  // Distinct bytes in every position show a mixed-up order at once.
  std::vector<uint32_t> words;
  for (uint32_t i = 0; i < 16; ++i) words.push_back(0x102030 + i * 0x010101);
  const std::vector<uint8_t> page = BootPage(words);
  Check(page.size() == 1 + 3 * 16 && page[0] == 1, "boot page of 16 words has length byte 1 and 49 bytes");
  Board board;
  Adsp2105 dsp(board);
  Check(dsp.BootPage(page.data(), page.size()) == page.size(), "boot page is consumed whole");
  bool same = true;
  for (uint32_t i = 0; i < 16; ++i) same = same && dsp.ProgramWord(i) == words[i];
  Check(same, "boot page words land in program memory 0-15 in order");
  Check(dsp.BootPage(page.data(), page.size() - 1) == 0, "a short boot page is refused");
}

void TestProgram() {
  // Encodings from MAME's decoder (adsp2100.cpp execute_run): load data
  // register immediate 0100 dddd dddd dddd dddd rrrr (AX0 = 0, AY0 = 4,
  // AR = 0Ah); conditional ALU to AR 0010001 z ffff yyxx x... with
  // function 3 = X + Y and condition Fh = always; write data memory
  // 1001 aaaa aaaa aaaa aa rrrr; jump 000110 aaaa aaaa aaaa aa cccc.
  const std::vector<uint8_t> page = BootPage({
      0x400050,  // AX0 = 5
      0x400034,  // AY0 = 3
      0x22600F,  // AR = AX0 + AY0
      0x93800A,  // DM(3800h) = AR
      0x18004F,  // JUMP 0004h
  });
  Board board;
  Adsp2105 dsp(board);
  dsp.BootPage(page.data(), page.size());
  dsp.Reset();
  dsp.Run(100);
  Check(dsp.DataWord(0x3800) == 8, "AX0 + AY0 = 8 stored to on-chip data memory");
  Check(dsp.Pc() == 4, "program ends in its jump-to-self at 0004h");
  Check(dsp.OutOfRangeAccesses() == 0, "no access outside the on-chip memories");
}

class SportBoard : public Adsp2105Board {
 public:
  void SportOutput(uint16_t sample, uint16_t) override { samples.push_back(sample); }
  std::vector<uint16_t> samples;
};

void TestSportAutobuffer() {
  // SPORT1 transmit autobuffering (manual 5.11): the first word comes from
  // TX1, then the SPORT takes words from DM(I0,M0) round a 4-word circular
  // buffer and raises the transmit interrupt (vector 0010h) at each wrap;
  // the handler counts wraps in DM(3810h). The main program sleeps in IDLE.
  // Encodings as in TestProgram, plus: group 1 immediate 001101 v14 rrrr
  // (I0 = 0, M0 = 4, L0 = 8), group 3 immediate 001111 v14 rrrr (IMASK = 3,
  // TX1 = 0Bh), read DM immediate 1000 00 a14 rrrr, ALU function 1 = Y + 1,
  // IDLE 028000h, RTI 0A001Fh.
  std::vector<uint32_t> program(0x40, 0);
  program[0x00] = 0x18020F;  // JUMP 0020h (past the vectors)
  program[0x10] = 0x838104;  // AY0 = DM(3810h)       SPORT1 transmit vector
  program[0x11] = 0x22200F;  // AR = AY0 + 1
  program[0x12] = 0x93810A;  // DM(3810h) = AR
  program[0x13] = 0x0A001F;  // RTI
  const uint32_t main_code[] = {
      0x411110, 0x938000,  // AX0 = 1111h; DM(3800h) = AX0
      0x422220, 0x938010,  // AX0 = 2222h; DM(3801h) = AX0
      0x433330, 0x938020,  // AX0 = 3333h; DM(3802h) = AX0
      0x444440, 0x938030,  // AX0 = 4444h; DM(3803h) = AX0
      0x378000,            // I0 = 3800h
      0x340048,            // L0 = 4
      0x340014,            // M0 = 1
      0x4000F0, 0x93FF20,  // AX0 = 000Fh; DM(3FF2h) = AX0   16-bit words
      0x400000, 0x93FF10,  // AX0 = 0;     DM(3FF1h) = AX0   SCLKDIV 0
      0x400020, 0x93FEF0,  // AX0 = 0002h; DM(3FEFh) = AX0   TBUF, I0, M0
      0x40C1F0, 0x93FFF0,  // AX0 = 0C1Fh; DM(3FFFh) = AX0   SPORT1 on
      0x3D555B,            // TX1 = 1555h
      0x3C0043,            // IMASK = 0004h (SPORT1 transmit)
      0x028000,            // IDLE                          (0035h)
      0x18035F,            // JUMP 0035h (back to IDLE)
  };
  for (std::size_t i = 0; i < sizeof main_code / sizeof main_code[0]; ++i) program[0x20 + i] = main_code[i];
  const std::vector<uint8_t> page = BootPage(program);
  SportBoard board;
  Adsp2105 dsp(board);
  dsp.BootPage(page.data(), page.size());
  dsp.Reset();
  dsp.Run(2000);  // About 30 words of 64 cycles (32-clock frame, SCLKDIV 0).
  const std::vector<uint16_t> expected = {0x1555, 0x1111, 0x2222, 0x3333, 0x4444, 0x1111, 0x2222};
  const bool order = board.samples.size() >= expected.size() &&
                     std::equal(expected.begin(), expected.end(), board.samples.begin());
  if (!order) {
    std::cout << std::hex << "  control 3FFF " << dsp.ControlRegister(0x3FFF) << " 3FF2 " << dsp.ControlRegister(0x3FF2)
              << " 3FEF " << dsp.ControlRegister(0x3FEF) << " DM 3800 " << dsp.DataWord(0x3800) << " PC " << dsp.Pc()
              << " idle " << dsp.Idle() << std::dec << "\n";
    std::cout << "  SPORT1 words:";
    for (std::size_t i = 0; i < board.samples.size() && i < 10; ++i) std::cout << " " << std::hex << board.samples[i] << std::dec;
    std::cout << "\n";
  }
  Check(order, "SPORT1 sends TX1, then the autobuffer round and round");
  const int wraps = dsp.DataWord(0x3810);
  const int expected_wraps = static_cast<int>(board.samples.size()) / 4;
  if (wraps < expected_wraps - 1 || wraps > expected_wraps + 1) {
    std::cout << "  " << board.samples.size() << " words, " << wraps << " interrupts\n";
  }
  Check(wraps >= expected_wraps - 1 && wraps <= expected_wraps + 1, "one transmit interrupt per buffer wrap");
  if (dsp.Pc() < 0x35 || dsp.Pc() > 0x36) {
    std::cout << "  PC " << std::hex << dsp.Pc() << " control 3FFF " << dsp.ControlRegister(0x3FFF) << " 3FEF "
              << dsp.ControlRegister(0x3FEF) << std::dec << "\n";
  }
  Check(dsp.Pc() >= 0x35 && dsp.Pc() <= 0x36, "main program waits in its IDLE loop");
}

}  // namespace

int main() {
  TestBootOrder();
  TestProgram();
  TestSportAutobuffer();
  std::cout << (failures == 0 ? "PASS" : "FAIL") << " dsp_selftest\n";
  return failures == 0 ? 0 : 1;
}
