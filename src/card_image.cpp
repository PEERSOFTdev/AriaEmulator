#include "card_image.h"

#include <fstream>

#include "card_folder.h"

namespace fs = std::filesystem;

namespace card_image {

bool ValidSize(uint64_t bytes) {
  return bytes >= FolderCard::kMinSize && bytes <= FolderCard::kMaxSize && (bytes & (bytes - 1)) == 0;
}

bool Read(const fs::path& file, std::vector<uint8_t>& image, std::wstring& error) {
  std::error_code ec;
  const uintmax_t bytes = fs::file_size(file, ec);
  if (ec) {
    error = L"Obraz karty „" + file.wstring() + L"“ nelze přečíst.";
    return false;
  }
  if (!ValidSize(bytes)) {
    error = L"Soubor „" + file.wstring() + L"“ nemá velikost žádné karty: má " + std::to_wstring(bytes) +
            L" bajtů, karta má mocninu dvou od 64 KB do 64 MB.";
    return false;
  }
  std::ifstream input(file, std::ios::binary);
  image.resize(static_cast<std::size_t>(bytes));
  input.read(reinterpret_cast<char*>(image.data()), static_cast<std::streamsize>(image.size()));
  if (!input) {
    error = L"Obraz karty „" + file.wstring() + L"“ nelze přečíst.";
    return false;
  }
  return true;
}

bool Write(const fs::path& file, const std::vector<uint8_t>& image, std::wstring& error) {
  const fs::path temporary = file.wstring() + L".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    if (!output) {
      error = L"Obraz karty nelze zapsat do „" + temporary.wstring() + L"“.";
      std::error_code ignored;
      output.close();
      fs::remove(temporary, ignored);
      return false;
    }
  }
  std::error_code ec;
  fs::rename(temporary, file, ec);
  if (ec) {
    error = L"Obraz karty nelze uložit do „" + file.wstring() + L"“.";
    return false;
  }
  return true;
}

}  // namespace card_image
