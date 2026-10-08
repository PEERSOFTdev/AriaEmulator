// The Aria emulator: loads drive B: (a firmware update, HANDOFF 1) and the
// FreeDOS kernel (HANDOFF 6.10), and runs the machine with its sound; the
// host keyboard plays the Aria's keys (HANDOFF 6.4).

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

EmulatorThread* g_emulator = nullptr;

// A host key as a key of the Aria's matrix (HANDOFF 6.4, from BKB.COM's
// decoding at +02E5h-+0489h): row 0 = port 0581h (bits 0-5 dots 3, 2, 1,
// 4, 5, 6, the keys' order from left to right; bit 7 the space bar), row
// 1 = 0582h (bits 0-7 F1-F8), row 2 = 0584h (bits 0-3 the cursor cross up,
// down, left, right; bit 6 the central Shift). The dot order shows in BKB's
// chord list (+052Eh-+0578h): space + 1Bh and 1Ch are API.DOC's time
// (dots 2345) and date (dots 145) only this way round. The host layout
// follows the Eureka emulator: F D S and J K L are dots 1-3 and 4-6. Home, End, PgUp and PgDn press two arrows at once,
// as the Aria API defines them (Home = up + left, PgDn = down + right).
struct AriaKey {
  int row = -1;
  uint8_t bits = 0;
};

AriaKey KeyFor(WPARAM key) {
  switch (key) {
    case 'F': return {0, 0x04};
    case 'D': return {0, 0x02};
    case 'S': return {0, 0x01};
    case 'J': return {0, 0x08};
    case 'K': return {0, 0x10};
    case 'L': return {0, 0x20};
    case VK_SPACE: return {0, 0x80};
    case VK_UP: return {2, 0x01};
    case VK_DOWN: return {2, 0x02};
    case VK_LEFT: return {2, 0x04};
    case VK_RIGHT: return {2, 0x08};
    case VK_HOME: return {2, 0x05};
    case VK_PRIOR: return {2, 0x09};
    case VK_END: return {2, 0x06};
    case VK_NEXT: return {2, 0x0A};
    case VK_SHIFT: return {2, 0x40};
    default:
      if (key >= VK_F1 && key <= VK_F8) return {1, static_cast<uint8_t>(1 << (key - VK_F1))};
      return {};
  }
}

// Host keys currently down, so that losing focus can release them all.
std::vector<WPARAM> g_held;

void ReleaseAll() {
  for (WPARAM key : g_held) {
    const AriaKey aria = KeyFor(key);
    if (aria.row >= 0 && g_emulator) g_emulator->Key(aria.row, aria.bits, false);
  }
  g_held.clear();
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
      // Alt+F4 stays the window's: the Aria has no Alt key.
      if (wparam == VK_F4 && (GetKeyState(VK_MENU) & 0x8000)) break;
      const AriaKey aria = KeyFor(wparam);
      if (aria.row < 0 || !g_emulator) break;
      // Windows' typematic repeat is not the Aria's; BKB repeats by itself.
      if (down && (lparam & (1 << 30))) return 0;
      if (down) {
        g_held.push_back(wparam);
      } else {
        std::erase(g_held, wparam);
      }
      g_emulator->Key(aria.row, aria.bits, down);
      return 0;
    }
    case WM_KILLFOCUS:
      ReleaseAll();
      break;
    case WM_DESTROY:
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
  // encoding, and where the keys go.
  const std::wstring version = image.version.empty() ? std::wstring(L"neznámá verze") : image.version;
  std::wstring title = L"Aria " + version + L", " + EncodingName(image.encoding) + L" – klávesy vedou do Arie";
  HWND window = CreateWindowExW(0, window_class.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, 640, 400, nullptr, nullptr, instance, nullptr);
  ShowWindow(window, show);

  EmulatorThread emulator(std::make_unique<AriaMachine>(image, std::move(kernel)));
  g_emulator = &emulator;
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
  g_emulator = nullptr;
  emulator.Stop();
  return 0;
}
