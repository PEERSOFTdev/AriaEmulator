// The Aria emulator: loads drive B: (a firmware update, HANDOFF 1) and the
// FreeDOS kernel (HANDOFF 6.10), and runs the machine with its sound. Keys
// do not reach the machine yet.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "aria_machine.h"
#include "emulator_thread.h"
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

std::wstring ExeFolder() {
  wchar_t path[MAX_PATH];
  const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring folder(path, length);
  const std::size_t slash = folder.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L"." : folder.substr(0, slash);
}

// Order: --jadro, then KERNEL.SYS in ARIA_FREEDOS, then next to the EXE.
// The kernel is FreeDOS under the GPL and ships as a separate file
// (HANDOFF 6.10).
std::wstring DefaultKernelPath() {
  const std::wstring folder = Environment(L"ARIA_FREEDOS");
  if (!folder.empty()) return folder + L"\\KERNEL.SYS";
  return ExeFolder() + L"\\KERNEL.SYS";
}

bool ReadFile(const std::wstring& path, std::vector<uint8_t>& bytes) {
  std::ifstream file(std::filesystem::path(path), std::ios::binary);
  if (!file) return false;
  bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return !bytes.empty();
}

const wchar_t* kTitle = L"Aria Emulator";

const wchar_t* kUsage =
    L"Použití: AriaEmulator.exe [--flash CESTA] [--kodovani kamenicky|cp852] [--jadro KERNEL.SYS]\n\n"
    L"CESTA je aktualizace Arie tak, jak ji Robotron dodával: archiv ZIP "
    L"(kartový i sériový), FLASHDSK.IMG, jeden z dílů ARIAA4.1 až ARIAA4.3, "
    L"nebo složka s nimi. Bez --flash se použije proměnná ARIAFLASH, jinak "
    L"CARD\\CC0705.ZIP ve složce z proměnné ARIAFW (výchozí D:\\aria\\update).\n\n"
    L"KERNEL.SYS je jádro FreeDOS (sestavení ke2046 pro 8086). Bez --jadro se "
    L"hledá ve složce z proměnné ARIA_FREEDOS, jinak vedle emulátoru.";

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  int count = 0;
  wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
  std::wstring flash_path, kernel_path;
  TextEncoding forced = TextEncoding::kUnknown;
  for (int index = 1; index < count; ++index) {
    const std::wstring argument = arguments[index];
    if (argument == L"--flash" && index + 1 < count) {
      flash_path = arguments[++index];
    } else if (argument == L"--jadro" && index + 1 < count) {
      kernel_path = arguments[++index];
    } else if (argument == L"--kodovani" && index + 1 < count &&
               ParseEncodingName(arguments[index + 1], forced)) {
      ++index;
    } else {
      LocalFree(arguments);
      MessageBoxW(nullptr, kUsage, kTitle, MB_OK | MB_ICONINFORMATION);
      return 2;
    }
  }
  LocalFree(arguments);
  if (flash_path.empty()) flash_path = DefaultFlashPath();
  if (kernel_path.empty()) kernel_path = DefaultKernelPath();

  FlashImage image;
  std::wstring error;
  if (!LoadFlashImage(flash_path, forced, image, error)) {
    MessageBoxW(nullptr, (error + L"\n\n" + kUsage).c_str(), kTitle, MB_OK | MB_ICONERROR);
    return 1;
  }
  std::vector<uint8_t> kernel;
  if (!ReadFile(kernel_path, kernel)) {
    MessageBoxW(nullptr, (L"Nepodařilo se načíst jádro FreeDOS: " + kernel_path + L"\n\n" + kUsage).c_str(), kTitle,
                MB_OK | MB_ICONERROR);
    return 1;
  }

  WNDCLASSW window_class{};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // IDC_ARROW
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  window_class.lpszClassName = L"AriaEmulatorWindow";
  RegisterClassW(&window_class);

  // The title carries the state for NVDA+T: the firmware build, its
  // encoding, and that keys do not reach the machine yet.
  const std::wstring version = image.version.empty() ? std::wstring(L"neznámá verze") : image.version;
  std::wstring title = L"Aria " + version + L", " + EncodingName(image.encoding) + L" – klávesy zatím nevedou do Arie";
  HWND window = CreateWindowExW(0, window_class.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, 640, 400, nullptr, nullptr, instance, nullptr);
  ShowWindow(window, show);

  EmulatorThread emulator(std::make_unique<AriaMachine>(image, std::move(kernel)));
  emulator.Start();
  // A slow timer so that the loop below looks at the audio state even when
  // nothing happens to the window.
  SetTimer(window, 1, 500, nullptr);

  MSG message;
  bool audio_reported = false;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!audio_reported && !emulator.AudioOk()) {
      // No sound device: say so in a way that is not the Aria's voice.
      audio_reported = true;
      MessageBeep(MB_ICONHAND);
      SetWindowTextW(window, (title + L" – bez zvuku: zvukové zařízení se nepodařilo otevřít").c_str());
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  emulator.Stop();
  return 0;
}
