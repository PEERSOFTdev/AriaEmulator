// The Aria emulator: loads drive B: (a firmware update, HANDOFF 1) and the
// FreeDOS kernel (HANDOFF 6.10), and runs the machine with its sound in a
// window after the Eureka A4 emulator's (main_window.h), where the host
// keyboard plays the Aria's keys (HANDOFF 6.4).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
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
#include "main_window.h"
#include "snapshot.h"
#include "text_codec.h"
#include "zip_reader.h"

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

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int show) {
  // The shell's folder picker (win::PickFolder) is a COM object.
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
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

  MachineInfo info;
  info.version = image.version.empty() ? std::wstring(L"neznámá verze") : image.version;
  info.encoding = EncodingName(image.encoding);
  info.text_encoding = image.encoding;
  info.flash = flash_path;
  info.kernel = kernel_path;

  // What the battery kept since the last run of this firmware image
  // (snapshot.h). Read and deleted: a run that ends without switching the
  // Aria off leaves nothing behind, so the next one starts as after a cut
  // battery.
  auto machine = std::make_unique<AriaMachine>(image, std::move(kernel));
  const uint32_t firmware = Crc32(image.bytes.data(), image.bytes.size());
  const std::filesystem::path memory_file = snapshot::FileFor(firmware);
  {
    std::vector<uint8_t> state;
    std::wstring snapshot_error;
    std::error_code ec;
    switch (snapshot::Load(memory_file, firmware, AriaMachine::kStateSize, state, snapshot_error)) {
      case snapshot::Result::kOk:
        machine->LoadState(state);
        std::filesystem::remove(memory_file, ec);
        break;
      case snapshot::Result::kMissing:
        break;
      case snapshot::Result::kCorrupt: {
        // The user's call. No: quit and leave the file for a look.
        const std::wstring question = snapshot_error + L"\r\n\r\nSpustit Arii s prázdnou pamětí?";
        if (MessageBoxW(nullptr, question.c_str(), kTitle, MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
        std::filesystem::remove(memory_file, ec);
        break;
      }
    }
  }

  EmulatorThread emulator(std::move(machine));
  MainWindow window(emulator, std::move(info));
  if (!window.Create()) {
    MessageBoxW(nullptr, L"Nepodařilo se vytvořit okno emulátoru.", kTitle, MB_OK | MB_ICONERROR);
    return 1;
  }
  window.Show(show);
  emulator.Start();
  win::RunMessageLoop(window.handle(), window.accelerators(), window.hostAccelerators(),
                      [&window] { return window.HostShortcutsActive(); });
  emulator.Stop();
  // The card is the user's files, kept by its own battery: it goes back to
  // its folder whatever happens to the memory.
  window.SaveCardOnExit();
  if (window.SaveMemoryOnExit()) {
    std::vector<uint8_t> state;
    emulator.machine().SaveState(state);
    std::wstring snapshot_error;
    if (!snapshot::Save(memory_file, firmware, state, snapshot_error)) {
      MessageBoxW(nullptr, snapshot_error.c_str(), kTitle, MB_OK | MB_ICONERROR);
    }
  }
  return 0;
}
