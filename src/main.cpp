// The Aria emulator: loads drive B: (a firmware update, HANDOFF 1) and the
// FreeDOS kernel (HANDOFF 6.10), and runs the machine with its sound in a
// window after the Eureka A4 emulator's (main_window.h), where the host
// keyboard plays the Aria's keys (HANDOFF 6.4).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <cwctype>
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
#include "settings.h"
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

std::filesystem::path ExeFolder() {
  wchar_t path[MAX_PATH];
  const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
  return std::filesystem::path(std::wstring(path, length)).parent_path();
}

// Where a user puts what the emulator needs: beside the EXE, the folder
// above it (Spustit-Ariu.bat beside bin\, a release unpacked one level
// down) and the working folder, as the Eureka emulator looks for its ROM.
std::vector<std::filesystem::path> SearchFolders() {
  std::vector<std::filesystem::path> folders;
  const auto add = [&folders](const std::filesystem::path& folder) {
    if (folder.empty()) return;
    for (const auto& known : folders) {
      if (Settings::SamePath(known.wstring(), folder.wstring())) return;
    }
    folders.push_back(folder);
  };
  const std::filesystem::path exe = ExeFolder();
  add(exe);
  add(exe.parent_path());
  std::error_code ec;
  add(std::filesystem::current_path(ec));
  return folders;
}

// "7.05" as 705; a build whose label has no number ranks below any that has.
int VersionRank(const std::wstring& version) {
  int rank = 0;
  bool any = false;
  for (wchar_t c : version) {
    if (c < L'0' || c > L'9') continue;
    rank = rank * 10 + (c - L'0');
    any = true;
  }
  return any ? rank : -1;
}

// The files in `folder` that may be drive B:, sorted because directory
// order is not defined. Only the names an update came under: any ZIP (the
// archive names differ per build and language), FLASHDSK.IMG and the first
// serial part. Not any *.IMG, because card images live beside the EXE too
// and a FAT12 card with a CONFIG.SYS would pass for B:. The size cap keeps a
// large unrelated ZIP from being read whole; updates are about 1.5 MB.
std::vector<std::filesystem::path> FlashFilesIn(const std::filesystem::path& folder) {
  constexpr std::uintmax_t kLargest = 8u << 20;
  std::vector<std::filesystem::path> files;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code size_ec;
    if (!it->is_regular_file(size_ec) || it->file_size(size_ec) > kLargest || size_ec) continue;
    std::wstring name = it->path().filename().wstring();
    for (wchar_t& c : name) c = static_cast<wchar_t>(std::towupper(c));
    if (name == L"FLASHDSK.IMG" || name == L"ARIAA4.1" || name.ends_with(L".ZIP")) files.push_back(it->path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

// Drive B: when neither --flash nor ARIAFLASH names it: the one of the last
// run while it still loads, otherwise the newest build in the first search
// folder that holds any. A folder counts with its CARD and SERIAL
// subfolders, the layout Robotron's update set came in. ARIAFW, when set, is
// the developer's set (CLAUDE.md), searched last; a release build carries no
// default path, since one from a developer's machine means nothing on
// anyone else's. Files are
// told by their contents (LoadFlashImage), so a FreeDOS ZIP or the
// emulator's own release ZIP beside the EXE is passed over.
bool FindFlash(const std::wstring& remembered, TextEncoding forced, FlashImage& image,
               std::vector<std::filesystem::path>& searched) {
  std::wstring ignored;
  if (!remembered.empty() && LoadFlashImage(remembered, forced, image, ignored)) return true;
  searched = SearchFolders();
  std::wstring developer = Environment(L"ARIAFW");
  for (const auto& folder : searched) {
    if (Settings::SamePath(folder.wstring(), developer)) developer.clear();
  }
  bool found = false;
  const auto search = [&](const std::filesystem::path& folder) {
    for (const auto& sub : {std::filesystem::path(), std::filesystem::path(L"CARD"), std::filesystem::path(L"SERIAL")}) {
      for (const auto& file : FlashFilesIn(folder / sub)) {
        FlashImage candidate;
        if (!LoadFlashImage(file.wstring(), forced, candidate, ignored)) continue;
        if (!found || VersionRank(candidate.version) > VersionRank(image.version)) image = std::move(candidate);
        found = true;
      }
    }
  };
  for (const auto& folder : searched) {
    search(folder);
    if (found) return true;
  }
  if (!developer.empty()) search(developer);
  return found;
}

// Order: --jadro, then KERNEL.SYS in ARIA_FREEDOS, then the search folders.
// The kernel is FreeDOS under the GPL and ships as a separate file
// (HANDOFF 6.10). Empty when none is there.
std::wstring FindKernel() {
  const std::wstring folder = Environment(L"ARIA_FREEDOS");
  if (!folder.empty()) return folder + L"\\KERNEL.SYS";
  std::error_code ec;
  for (const auto& candidate : SearchFolders()) {
    if (std::filesystem::is_regular_file(candidate / L"KERNEL.SYS", ec)) return (candidate / L"KERNEL.SYS").wstring();
  }
  return {};
}

std::wstring FolderList(const std::vector<std::filesystem::path>& folders) {
  std::wstring list;
  for (const auto& folder : folders) list += L"\n" + folder.wstring();
  return list;
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
    L"nebo složka s nimi. Bez --flash se použije firmware z minulého spuštění, "
    L"jinak se hledá vedle emulátoru, o složku výš a v pracovní složce; "
    L"z několika verzí vyhraje nejnovější.\n\n"
    L"KERNEL.SYS je jádro FreeDOS (sestavení ke2046 pro 8086). Bez --jadro se "
    L"hledá na stejných místech.";

const wchar_t* kWhatToPut =
    L"Zkopírujte do složky s AriaEmulator.exe aktualizaci Arie tak, jak ji Robotron "
    L"dodával, například CC0705.ZIP nebo CSR0687.ZIP (stačí kterýkoli z archivů "
    L"ZIP), případně FLASHDSK.IMG nebo díly ARIAA4.1 až ARIAA4.3.";

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
  // Before the image: it says which drive B: the last run had.
  Settings settings(Settings::FindFile());
  settings.Load();

  // A path given (--flash, or ARIAFLASH for the developer) is the user's
  // word, and a failure there is said rather than searched past.
  if (flash_path.empty()) flash_path = Environment(L"ARIAFLASH");
  FlashImage image;
  std::wstring error;
  if (!flash_path.empty()) {
    if (!LoadFlashImage(flash_path, forced, image, error)) {
      MessageBoxW(nullptr, (error + L"\n\n" + kUsage).c_str(), kTitle, MB_OK | MB_ICONERROR);
      return 1;
    }
  } else {
    std::vector<std::filesystem::path> searched;
    if (!FindFlash(settings.flash(), forced, image, searched)) {
      const std::wstring message = L"Firmware Arie (obsah disku B:) se nenašel. Hledal se v těchto složkách:" +
                                   FolderList(searched) + L"\n\n" + kWhatToPut;
      MessageBoxW(nullptr, message.c_str(), kTitle, MB_OK | MB_ICONERROR);
      return 1;
    }
    flash_path = image.origin;
  }
  // Absolute, or a --flash given relative to one working folder would be
  // lost from the next. Not std::filesystem::absolute: MinGW's prefixes an
  // already absolute UNC path (\\server\share) with the share once more.
  {
    wchar_t full[MAX_PATH * 2];
    const DWORD length = GetFullPathNameW(flash_path.c_str(), static_cast<DWORD>(std::size(full)), full, nullptr);
    if (length > 0 && length < std::size(full)) flash_path.assign(full, length);
  }
  if (!Settings::SamePath(settings.flash(), flash_path)) {
    settings.SetFlash(flash_path);
    // Not worth stopping for: the next run searches again.
    std::wstring ignored;
    settings.Save(ignored);
  }

  if (kernel_path.empty()) kernel_path = FindKernel();
  if (kernel_path.empty()) {
    const std::wstring message =
        L"Jádro FreeDOS KERNEL.SYS se nenašlo. Hledalo se v těchto složkách:" + FolderList(SearchFolders()) +
        L"\n\nZkopírujte do složky s AriaEmulator.exe soubor KERNEL.SYS ze sestavení ke2046 pro 8086 "
        L"(archiv ke2046_86f16.zip z projektu FDOS/kernel na GitHubu).";
    MessageBoxW(nullptr, message.c_str(), kTitle, MB_OK | MB_ICONERROR);
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
  MainWindow window(emulator, std::move(info), settings);
  if (!window.Create()) {
    MessageBoxW(nullptr, L"Nepodařilo se vytvořit okno emulátoru.", kTitle, MB_OK | MB_ICONERROR);
    return 1;
  }
  window.Show(show);
  // The card and the diskette the last run ended with, in before the
  // machine starts, as they would have stayed in; the Backpack's cable too,
  // so BACKPACK.SYS finds the drive at start-up.
  window.InsertRememberedMedia();
  emulator.Start();
  win::RunMessageLoop(window.handle(), window.accelerators(), window.hostAccelerators(),
                      [&window] { return window.HostShortcutsActive(); });
  emulator.Stop();
  // The card and the diskette are the user's files: they go back home
  // whatever happens to the memory.
  window.SaveMediaOnExit();
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
