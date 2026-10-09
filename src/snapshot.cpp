#include "snapshot.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "zip_reader.h"

namespace fs = std::filesystem;

namespace snapshot {
namespace {

// "ARIAPAM1", the firmware's CRC-32, the length of what follows, the state,
// and a CRC-32 of the state, all little-endian.
constexpr char kMagic[8] = {'A', 'R', 'I', 'A', 'P', 'A', 'M', '1'};
constexpr std::size_t kHeader = 16;

void Put32(std::vector<uint8_t>& out, uint32_t value) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

uint32_t Get32(const uint8_t* in) {
  return static_cast<uint32_t>(in[0]) | static_cast<uint32_t>(in[1]) << 8 | static_cast<uint32_t>(in[2]) << 16 |
         static_cast<uint32_t>(in[3]) << 24;
}

fs::path ExecutableDirectory() {
  std::wstring buffer(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  buffer.resize(length);
  return fs::path(buffer).parent_path();
}

// %APPDATA%, asked of the shell rather than the environment, which is
// inherited and can arrive wrong from whatever launched us (Eureka).
fs::path RoamingFolder() {
  PWSTR path = nullptr;
  if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path) != S_OK) return {};
  fs::path result(path);
  CoTaskMemFree(path);
  return result;
}

}  // namespace

fs::path ConfigDirectory() {
  std::error_code ec;
  const fs::path portable = ExecutableDirectory() / L"config";
  if (fs::is_directory(portable, ec)) return portable;
  const fs::path roaming = RoamingFolder();
  if (roaming.empty()) return {};
  return roaming / L"Aria";
}

fs::path FileFor(uint32_t firmware) {
  const fs::path dir = ConfigDirectory();
  if (dir.empty()) return {};
  wchar_t name[32];
  std::swprintf(name, std::size(name), L"pamet-%08X.bin", firmware);
  return dir / name;
}

Result Load(const fs::path& path, uint32_t firmware, std::size_t size, std::vector<uint8_t>& state,
            std::wstring& error) {
  std::error_code ec;
  if (path.empty() || !fs::exists(path, ec)) return Result::kMissing;
  std::ifstream file(path, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  const bool sound = file.good() || file.eof();
  if (!sound || bytes.size() != kHeader + size + 4 || !std::equal(kMagic, kMagic + 8, bytes.begin()) ||
      Get32(bytes.data() + 8) != firmware || Get32(bytes.data() + 12) != size ||
      Get32(bytes.data() + kHeader + size) != Crc32(bytes.data() + kHeader, size)) {
    error = L"Uloženou paměť Arie se nepodařilo přečíst, soubor je poškozený: " + path.wstring();
    return Result::kCorrupt;
  }
  state.assign(bytes.begin() + kHeader, bytes.begin() + static_cast<std::ptrdiff_t>(kHeader + size));
  return Result::kOk;
}

bool Save(const fs::path& path, uint32_t firmware, const std::vector<uint8_t>& state, std::wstring& error) {
  if (path.empty()) {
    error = L"Paměť Arie se nedá uložit: systém neřekl, kde je složka dat aplikací.";
    return false;
  }
  std::error_code ec;
  // The roaming folder is ours to make; the portable `config` folder exists
  // by now or ConfigDirectory would not have chosen it.
  fs::create_directories(path.parent_path(), ec);
  std::vector<uint8_t> bytes(kMagic, kMagic + 8);
  Put32(bytes, firmware);
  Put32(bytes, static_cast<uint32_t>(state.size()));
  bytes.insert(bytes.end(), state.begin(), state.end());
  Put32(bytes, Crc32(state.data(), state.size()));
  fs::path temporary = path;
  temporary += L".tmp";
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file.good()) {
      error = L"Paměť Arie se nepodařilo uložit do " + temporary.wstring();
      fs::remove(temporary, ec);
      return false;
    }
  }
  fs::rename(temporary, path, ec);
  if (ec) {
    error = L"Paměť Arie se nepodařilo uložit do " + path.wstring();
    fs::remove(temporary, ec);
    return false;
  }
  return true;
}

}  // namespace snapshot
