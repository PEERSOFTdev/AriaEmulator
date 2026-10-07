// Disassembles an ADSP-2105 program from a boot page file such as
// SYSTEM\LPC.DSP (HANDOFF 6.3), with MAME's disassembler. For analysis
// only: the output is firmware, so it belongs outside the repository.
//
//   dsp_disasm FILE.DSP [FROM [TO]]   (word addresses, hexadecimal)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

#include "adsp2100/2100dasm.h"

int wmain(int argc, wchar_t** argv) {
  if (argc < 2) {
    std::cout << "usage: dsp_disasm FILE.DSP [FROM [TO]]\n";
    return 2;
  }
  std::ifstream file(argv[1], std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    std::cout << "cannot read the file\n";
    return 1;
  }
  // Boot page layout (ADSP-2100 Family User's Manual 10.4.4): length byte,
  // then the words from the last to word 0, each upper, lower, middle byte.
  const std::size_t count = (static_cast<std::size_t>(bytes[0]) + 1) * 8;
  if (bytes.size() < 1 + 3 * count) {
    std::cout << "shorter than its boot page length says\n";
    return 1;
  }
  std::vector<uint32_t> words(count);
  for (std::size_t k = 0; k < count; ++k) {
    const uint8_t* b = &bytes[1 + 3 * k];
    words[count - 1 - k] = static_cast<uint32_t>(b[0]) << 16 | static_cast<uint32_t>(b[2]) << 8 | b[1];
  }
  uint32_t from = argc > 2 ? std::wcstoul(argv[2], nullptr, 16) : 0;
  uint32_t to = argc > 3 ? std::wcstoul(argv[3], nullptr, 16) : static_cast<uint32_t>(count - 1);
  if (to >= count) to = static_cast<uint32_t>(count - 1);

  mame_adsp::adsp21xx_disassembler disassembler;
  const mame_adsp::data_buffer buffer(words.data(), static_cast<uint32_t>(words.size()));
  std::printf("; %zu words\n", count);
  for (uint32_t pc = from; pc <= to; ++pc) {
    std::ostringstream text;
    disassembler.disassemble(text, pc, buffer, buffer);
    std::printf("%04X  %06X  %s\n", pc, words[pc], text.str().c_str());
  }
  return 0;
}
