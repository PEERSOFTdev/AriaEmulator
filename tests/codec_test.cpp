// Kamenicky and CP852 round trips, plus the exact bytes of one message from
// each Czech firmware family (HANDOFF 1). Prints PASS on success.

#include <iostream>
#include <string>
#include <vector>

#include "text_codec.h"

namespace {

bool RoundTrip(TextEncoding encoding, const std::wstring& text) {
  const auto bytes = Encode(encoding, text);
  return Decode(encoding, bytes.data(), bytes.size()) == text;
}

}  // namespace

int main() {
  bool passed = true;
  const std::wstring czech = L"Příliš žluťoučký kůň úpěl ďábelské ódy, ĎŤŇ Ů";
  passed &= RoundTrip(TextEncoding::kKamenicky, czech);
  passed &= RoundTrip(TextEncoding::kCp852, czech);

  // "prazdny radek" with diacritics: 6.87 stores y-acute as 0x98 and
  // r-caron as 0xA9 (Kamenicky), 7.05 as 0xEC and 0xFD (CP852).
  const std::vector<uint8_t> kamenicky = {'p', 'r', 0xA0, 'z', 'd', 'n', 0x98, ' ',
                                          0xA9, 0xA0, 'd', 'e', 'k'};
  const std::vector<uint8_t> cp852 = {'p', 'r', 0xA0, 'z', 'd', 'n', 0xEC, ' ',
                                      0xFD, 0xA0, 'd', 'e', 'k'};
  passed &= Encode(TextEncoding::kKamenicky, L"prázdný řádek") == kamenicky;
  passed &= Encode(TextEncoding::kCp852, L"prázdný řádek") == cp852;
  passed &= Decode(TextEncoding::kCp852, cp852.data(), cp852.size()) == L"prázdný řádek";

  // Characters outside the table fall back to their base letter.
  const auto folded = Encode(TextEncoding::kKamenicky, L"A–B… ‘text’ ő");
  passed &= Decode(TextEncoding::kKamenicky, folded.data(), folded.size()) == L"A-B... 'text' o";
  const auto latin2 = Encode(TextEncoding::kCp852, L"ő ł ę");
  passed &= latin2 == std::vector<uint8_t>{0x8B, ' ', 0x88, ' ', 0xA9};

  TextEncoding parsed = TextEncoding::kUnknown;
  passed &= ParseEncodingName(L"kamenicky", parsed) && parsed == TextEncoding::kKamenicky;
  passed &= ParseEncodingName(L"latin2", parsed) && parsed == TextEncoding::kCp852;
  passed &= !ParseEncodingName(L"cp437", parsed);

  std::cout << (passed ? "PASS" : "FAIL") << " codec_test\n";
  return passed ? 0 : 1;
}
