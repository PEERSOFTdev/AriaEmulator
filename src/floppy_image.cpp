#include "floppy_image.h"

#include <fstream>

namespace fs = std::filesystem;

namespace floppy_image {

namespace {

constexpr uintmax_t kLargest = 1474560;  // 1.44 MB.

}  // namespace

bool Read(const fs::path& file, Fdc82077::Disk& disk, std::wstring& error) {
  std::error_code ec;
  const uintmax_t bytes = fs::file_size(file, ec);
  if (ec) {
    error = L"Obraz diskety „" + file.wstring() + L"“ nelze přečíst.";
    return false;
  }
  if (bytes == 0) {
    error = L"Soubor „" + file.wstring() + L"“ je prázdný, žádná disketa v něm není.";
    return false;
  }
  if (bytes > kLargest) {
    error = L"Soubor „" + file.wstring() + L"“ má " + std::to_wstring(bytes) +
            L" bajtů, víc než disketa 1,44 MB. Mechanika Backpack bere diskety 3,5\" 720 KB a 1,44 MB.";
    return false;
  }
  std::vector<uint8_t> image(static_cast<std::size_t>(bytes));
  std::ifstream input(file, std::ios::binary);
  input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size()));
  if (!input) {
    error = L"Obraz diskety „" + file.wstring() + L"“ nelze přečíst.";
    return false;
  }
  // A file the user may not write is a disk with its tab open: the Aria
  // can read it, and nothing is ever saved over it.
  const fs::perms perms = fs::status(file, ec).permissions();
  const bool read_only = !ec && (perms & fs::perms::owner_write) == fs::perms::none;
  std::optional<Fdc82077::Disk> loaded = Fdc82077::Disk::FromImage(std::move(image), read_only);
  if (!loaded) {
    // FromImage refuses only by the boot sector at this size: a BPB of
    // another layout, or one the file is longer than.
    error = L"Zaváděcí sektor „" + file.wstring() +
            L"“ neodpovídá disketě 720 KB ani 1,44 MB této velikosti: buď je to disketa 5,25\" (360 KB "
            L"nebo 1,2 MB), kterou mechanika Backpack 3,5\" nepřečte, nebo je soubor delší, než disketa podle "
            L"něj má být.";
    return false;
  }
  disk = std::move(*loaded);
  return true;
}

bool Write(const fs::path& file, const std::vector<uint8_t>& bytes, std::wstring& error) {
  const fs::path temporary = file.wstring() + L".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
      error = L"Obraz diskety nelze zapsat do „" + temporary.wstring() + L"“.";
      std::error_code ignored;
      output.close();
      fs::remove(temporary, ignored);
      return false;
    }
  }
  std::error_code ec;
  fs::rename(temporary, file, ec);
  if (ec) {
    error = L"Obraz diskety nelze uložit do „" + file.wstring() + L"“.";
    return false;
  }
  return true;
}

}  // namespace floppy_image
