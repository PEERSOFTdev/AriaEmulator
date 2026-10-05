// Phase 0 entry point: no machine yet. It loads the selected drive B:
// image and reports what it found, so that choosing a firmware build can
// be tried out before anything runs it.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <string>

#include "flash_image.h"
#include "text_codec.h"

namespace {

std::wstring Environment(const wchar_t* name) {
  wchar_t buffer[MAX_PATH * 2];
  const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
  if (length == 0 || length >= std::size(buffer)) return {};
  return std::wstring(buffer, length);
}

// Order: --flash on the command line, then ARIAFLASH, then the Czech 7.05
// card update inside ARIAFW (default D:\aria\update, CLAUDE.md).
std::wstring DefaultFlashPath() {
  std::wstring path = Environment(L"ARIAFLASH");
  if (!path.empty()) return path;
  std::wstring folder = Environment(L"ARIAFW");
  if (folder.empty()) folder = L"D:\\aria\\update";
  return folder + L"\\CARD\\CC0705.ZIP";
}

const wchar_t* SourceName(FlashSource source) {
  return source == FlashSource::kSerialParts ? L"sériová aktualizace" : L"kartová aktualizace";
}

const wchar_t* kUsage =
    L"Použití: AriaEmulator.exe [--flash CESTA] [--kodovani kamenicky|cp852]\n\n"
    L"CESTA je aktualizace Arie tak, jak ji Robotron dodával: archiv ZIP "
    L"(kartový i sériový), FLASHDSK.IMG, jeden z dílů ARIAA4.1 až ARIAA4.3, "
    L"nebo složka s nimi. Bez --flash se použije proměnná ARIAFLASH, jinak "
    L"CARD\\CC0705.ZIP ve složce z proměnné ARIAFW (výchozí D:\\aria\\update).";

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  int count = 0;
  wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
  std::wstring flash_path;
  TextEncoding forced = TextEncoding::kUnknown;
  for (int index = 1; index < count; ++index) {
    const std::wstring argument = arguments[index];
    if (argument == L"--flash" && index + 1 < count) {
      flash_path = arguments[++index];
    } else if (argument == L"--kodovani" && index + 1 < count &&
               ParseEncodingName(arguments[index + 1], forced)) {
      ++index;
    } else {
      LocalFree(arguments);
      MessageBoxW(nullptr, kUsage, L"Aria Emulator", MB_OK | MB_ICONINFORMATION);
      return 2;
    }
  }
  LocalFree(arguments);
  if (flash_path.empty()) flash_path = DefaultFlashPath();

  FlashImage image;
  std::wstring error;
  if (!LoadFlashImage(flash_path, forced, image, error)) {
    MessageBoxW(nullptr, (error + L"\n\n" + kUsage).c_str(), L"Aria Emulator",
                MB_OK | MB_ICONERROR);
    return 1;
  }

  std::wstring report = L"Disk B: načten.\n\n";
  report += L"Zdroj: " + image.origin + L" (" + SourceName(image.source) + L")\n";
  report += L"Verze: " + (image.version.empty() ? std::wstring(L"neznámá") : image.version) + L"\n";
  report += L"Kódování: " + std::wstring(EncodingName(image.encoding));
  if (image.encoding_forced) {
    report += L" (vnuceno, zjištěno " + std::wstring(EncodingName(image.detected)) + L")";
  } else if (image.detected == TextEncoding::kUnknown) {
    report += L" (odhad, z obrazu se nedalo poznat)";
  }
  report += L"\n\nEmulace stroje zatím není hotová.";
  MessageBoxW(nullptr, report.c_str(), L"Aria Emulator", MB_OK | MB_ICONINFORMATION);
  return 0;
}
