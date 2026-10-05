#include "text_codec.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <iterator>

namespace {

// Exact KEYBCS2/Kamenicky table, the same one the Eureka A4 emulator uses.
// Windows does not expose this historical character set as a code page.
constexpr wchar_t kKamenickyHigh[] =
    L"ČüéďäĎŤčěĚĹÍľĺÄÁÉžŽôöÓůÚýÖÜŠĽÝŘť"
    L"áíóúňŇŮÔšřŕŔ¼§«»"
    L"░▒▓│┤╡╢╖╕╣║╗╝╜╛┐"
    L"└┴┬├─┼╞╟╚╔╩╦╠═╬╧"
    L"╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀"
    L"αßΓπΣσµτΦΘΩδ∞φε∩"
    L"≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";
static_assert(std::size(kKamenickyHigh) == 129);

// CP852 upper half, generated from Python's cp852 codec rather than typed,
// and kept as a table instead of MultiByteToWideChar(852) so that encoding
// does not depend on which code pages a given Windows installation carries.
// 0xF0 is SOFT HYPHEN and 0xFF NO-BREAK SPACE, written as escapes because
// both are invisible.
constexpr wchar_t kCp852High[] =
    L"ÇüéâäůćçłëŐőîŹÄĆ"
    L"ÉĹĺôöĽľŚśÖÜŤťŁ×č"
    L"áíóúĄąŽžĘę¬źČş«»"
    L"░▒▓│┤ÁÂĚŞ╣║╗╝Żż┐"
    L"└┴┬├─┼Ăă╚╔╩╦╠═╬¤"
    L"đĐĎËďŇÍÎě┘┌█▄ŢŮ▀"
    L"ÓßÔŃńňŠšŔÚŕŰýÝţ´"
    L"\x00ad" L"˝˛ˇ˘§÷¸°¨˙űŘř■" L"\x00a0";
static_assert(std::size(kCp852High) == 129);

const wchar_t* HighTable(TextEncoding encoding) {
  return encoding == TextEncoding::kCp852 ? kCp852High : kKamenickyHigh;
}

bool EncodeOne(const wchar_t* table, wchar_t ch, uint8_t& output) {
  if (ch < 0x80) {
    output = static_cast<uint8_t>(ch);
    return true;
  }
  const wchar_t* end = table + 128;
  const wchar_t* found = std::find(table, end, ch);
  if (found == end) return false;
  output = static_cast<uint8_t>(0x80 + (found - table));
  return true;
}

void AppendReplacement(wchar_t ch, std::wstring& normalized) {
  switch (ch) {
    case 0x202f: normalized.push_back(L' '); break;
    case 0x2018: case 0x2019: case 0x201a: normalized.push_back(L'\''); break;
    case 0x201c: case 0x201d: case 0x201e: normalized.push_back(L'"'); break;
    case 0x2010: case 0x2011: case 0x2012: case 0x2013:
    case 0x2014: case 0x2212: normalized.push_back(L'-'); break;
    case 0x2026: normalized.append(L"..."); break;
    default: normalized.push_back(ch); break;
  }
}

}  // namespace

std::vector<uint8_t> Encode(TextEncoding encoding, std::wstring_view text) {
  const wchar_t* table = HighTable(encoding);
  std::wstring cleaned;
  cleaned.reserve(text.size());
  for (wchar_t ch : text) AppendReplacement(ch, cleaned);
  // Kamenicky has no NO-BREAK SPACE (0xFF there is a plain space glyph in
  // the Eureka table), CP852 does; fold it only where it cannot be encoded.
  if (encoding != TextEncoding::kCp852) {
    std::replace(cleaned.begin(), cleaned.end(), L'\x00a0', L' ');
  }

  std::vector<uint8_t> encoded;
  encoded.reserve(cleaned.size());
  for (wchar_t ch : cleaned) {
    uint8_t byte = 0;
    if (EncodeOne(table, ch, byte)) {
      encoded.push_back(byte);
      continue;
    }

    // For a character outside the table, drop its combining marks and use
    // the base letter where possible, so that modern Windows text stays
    // readable instead of turning into question marks.
    wchar_t decomposed[16]{};
    const int count = NormalizeString(NormalizationD, &ch, 1, decomposed,
                                      static_cast<int>(std::size(decomposed)));
    bool used = false;
    for (int index = 0; index < count && !used; ++index) {
      WORD type = 0;
      GetStringTypeW(CT_CTYPE3, &decomposed[index], 1, &type);
      if ((type & (C3_NONSPACING | C3_DIACRITIC)) != 0) continue;
      if (EncodeOne(table, decomposed[index], byte)) {
        encoded.push_back(byte);
        used = true;
      }
    }
    if (!used) encoded.push_back('?');
  }
  return encoded;
}

std::wstring Decode(TextEncoding encoding, const uint8_t* bytes,
                    std::size_t length) {
  const wchar_t* table = HighTable(encoding);
  std::wstring result;
  result.reserve(length);
  for (std::size_t index = 0; index < length; ++index) {
    const uint8_t byte = bytes[index];
    result.push_back(byte < 0x80 ? static_cast<wchar_t>(byte)
                                 : table[byte - 0x80]);
  }
  return result;
}

const wchar_t* EncodingName(TextEncoding encoding) {
  switch (encoding) {
    case TextEncoding::kKamenicky: return L"kamenicky";
    case TextEncoding::kCp852: return L"cp852";
    case TextEncoding::kUnknown: break;
  }
  return L"nezname";
}

bool ParseEncodingName(std::wstring_view name, TextEncoding& encoding) {
  if (name == L"kamenicky" || name == L"keybcs2" || name == L"cp895") {
    encoding = TextEncoding::kKamenicky;
    return true;
  }
  if (name == L"cp852" || name == L"latin2") {
    encoding = TextEncoding::kCp852;
    return true;
  }
  return false;
}
