// The ADSP-2105 facade (src/adsp2105.*) without firmware: boot page
// loading in the byte order of the ADSP-2100 Family User's Manual (10.4.4)
// and a short hand-encoded program run on MAME's core. Prints PASS on
// success.

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

}  // namespace

int main() {
  TestBootOrder();
  TestProgram();
  std::cout << (failures == 0 ? "PASS" : "FAIL") << " dsp_selftest\n";
  return failures == 0 ? 0 : 1;
}
