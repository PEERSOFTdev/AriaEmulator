#include "settings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <fstream>
#include <iterator>
#include <sstream>

#include "snapshot.h"

namespace fs = std::filesystem;

namespace {

// Keys are ASCII, so a key retyped without diacritics still matches.
constexpr char kLastCardKey[] = "posledni-karta";
constexpr char kImageFolderKey[] = "slozka-obrazu";
// "slot3=D:\Karty\Dopisy": a slot and its folder or image.
constexpr char kSlotPrefix[] = "slot";
// One card per line, numbered because the parser keeps the last value of a
// repeated key: "karta1=1048576;0;C:\Dopisy" -- size in bytes, 1 when
// locked, then the folder or image file, last, because a path may hold a
// ';'.
constexpr char kCardPrefix[] = "karta";
// The Backpack drive (HANDOFF 6.7): "backpack=1" when its cable is in, the
// diskette in it at the end, the folder of the last image, and one line a
// locked diskette, numbered for the same reason as the cards.
constexpr char kBackpackKey[] = "backpack";
constexpr char kLastDiskKey[] = "posledni-disketa";
constexpr char kDiskFolderKey[] = "slozka-disket";
constexpr char kLockedDiskPrefix[] = "zamcena-disketa";
constexpr char kDiskSlotPrefix[] = "disketa-slot";
constexpr char kBom[] = "\xef\xbb\xbf";

std::string ToUtf8(const std::wstring& text) {
  if (text.empty()) return {};
  const int length =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr,
                      nullptr);
  return result;
}

std::wstring FromUtf8(const std::string& text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
  return result;
}

std::string Trim(const std::string& text) {
  std::size_t begin = 0, end = text.size();
  while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
  while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
  return text.substr(begin, end - begin);
}

std::wstring Normalized(std::wstring path) {
  for (wchar_t& ch : path) {
    if (ch == L'/') ch = L'\\';
  }
  while (path.size() > 3 && path.back() == L'\\') path.pop_back();
  if (!path.empty()) CharLowerBuffW(path.data(), static_cast<DWORD>(path.size()));
  return path;
}

}  // namespace

fs::path Settings::FindFile() {
  const fs::path dir = snapshot::ConfigDirectory();
  return dir.empty() ? fs::path{} : dir / L"nastaveni.txt";
}

bool Settings::SamePath(const std::wstring& left, const std::wstring& right) {
  return Normalized(left) == Normalized(right);
}

const Settings::Card* Settings::FindCard(const std::wstring& path) const {
  for (const Card& card : cards_) {
    if (SamePath(card.path, path)) return &card;
  }
  return nullptr;
}

void Settings::RememberCard(const std::wstring& path, uint32_t size, bool locked) {
  for (Card& card : cards_) {
    if (SamePath(card.path, path)) {
      card.size = size;
      card.locked = locked;
      return;
    }
  }
  cards_.push_back({path, size, locked});
}

void Settings::SetCardLocked(const std::wstring& path, bool locked) {
  const Card* known = FindCard(path);
  RememberCard(path, known ? known->size : 0, locked);
}

bool Settings::DiskLocked(const std::wstring& path) const {
  for (const std::wstring& locked : lockedDisks_) {
    if (SamePath(locked, path)) return true;
  }
  return false;
}

void Settings::SetDiskLocked(const std::wstring& path, bool locked) {
  std::erase_if(lockedDisks_, [&](const std::wstring& known) { return SamePath(known, path); });
  if (locked) lockedDisks_.push_back(path);
}

void Settings::Load() {
  std::ifstream input(file_, std::ios::binary);
  if (!input) return;
  std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (bytes.starts_with(kBom)) bytes.erase(0, 3);
  std::istringstream lines(bytes);
  std::string line;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) continue;
    const std::string key = Trim(line.substr(0, equals));
    if (key.empty() || key.front() == '#') continue;
    const std::string value = Trim(line.substr(equals + 1));
    if (key == kLastCardKey) {
      lastCard_ = FromUtf8(value);
      continue;
    }
    if (key == kImageFolderKey) {
      imageFolder_ = FromUtf8(value);
      continue;
    }
    if (key == kBackpackKey) {
      backpackConnected_ = value == "1";
      continue;
    }
    if (key == kLastDiskKey) {
      lastDisk_ = FromUtf8(value);
      continue;
    }
    if (key == kDiskFolderKey) {
      diskFolder_ = FromUtf8(value);
      continue;
    }
    if (key.size() == std::size(kDiskSlotPrefix) && key.starts_with(kDiskSlotPrefix) && key.back() >= '1' &&
        key.back() <= '0' + kSlots) {
      SetDiskSlot(key.back() - '0', FromUtf8(value));
      continue;
    }
    if (key.starts_with(kLockedDiskPrefix)) {
      if (!value.empty()) SetDiskLocked(FromUtf8(value), true);
      continue;
    }
    if (key.size() == std::size(kSlotPrefix) && key.starts_with(kSlotPrefix) && key.back() >= '1' &&
        key.back() <= '0' + kSlots) {
      SetSlot(key.back() - '0', FromUtf8(value));
      continue;
    }
    if (key.starts_with(kCardPrefix)) {
      const std::size_t first = value.find(';');
      const std::size_t second = first == std::string::npos ? first : value.find(';', first + 1);
      if (second == std::string::npos) continue;
      const std::string size = value.substr(0, first);
      const std::string locked = value.substr(first + 1, second - first - 1);
      const std::wstring path = FromUtf8(value.substr(second + 1));
      if (size.empty() || size.size() > 10 || size.find_first_not_of("0123456789") != std::string::npos ||
          path.empty())
        continue;
      RememberCard(path, static_cast<uint32_t>(std::stoull(size)), locked == "1");
    }
  }
}

bool Settings::Save(std::wstring& error) const {
  if (file_.empty()) {
    error = L"Není kam uložit nastavení: systém neřekl, kde je složka aplikačních dat.";
    return false;
  }
  std::error_code ec;
  fs::create_directories(file_.parent_path(), ec);
  std::wstring text =
      L"# Nastavení emulátoru Aria.\r\n"
      L"# Soubor přepisuje emulátor, vlastní řádky v něm nepřežijí.\r\n"
      L"\r\n";
  if (!lastCard_.empty()) text += L"posledni-karta=" + lastCard_ + L"\r\n";
  if (!imageFolder_.empty()) text += L"slozka-obrazu=" + imageFolder_ + L"\r\n";
  for (int number = 1; number <= kSlots; ++number) {
    if (!slot(number).empty()) text += L"slot" + std::to_wstring(number) + L"=" + slot(number) + L"\r\n";
  }
  for (std::size_t i = 0; i < cards_.size(); ++i) {
    text += L"karta" + std::to_wstring(i + 1) + L"=" + std::to_wstring(cards_[i].size) + L";" +
            (cards_[i].locked ? L"1" : L"0") + L";" + cards_[i].path + L"\r\n";
  }
  text += std::wstring(L"backpack=") + (backpackConnected_ ? L"1" : L"0") + L"\r\n";
  if (!lastDisk_.empty()) text += L"posledni-disketa=" + lastDisk_ + L"\r\n";
  if (!diskFolder_.empty()) text += L"slozka-disket=" + diskFolder_ + L"\r\n";
  for (int number = 1; number <= kSlots; ++number) {
    if (!disk_slot(number).empty())
      text += L"disketa-slot" + std::to_wstring(number) + L"=" + disk_slot(number) + L"\r\n";
  }
  for (std::size_t i = 0; i < lockedDisks_.size(); ++i)
    text += L"zamcena-disketa" + std::to_wstring(i + 1) + L"=" + lockedDisks_[i] + L"\r\n";
  // Through a temporary file, so a failed write leaves the old settings.
  const fs::path temporary = file_.wstring() + L".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    const std::string bytes = std::string(kBom) + ToUtf8(text);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
      error = L"Nastavení nelze zapsat do „" + temporary.wstring() + L"“.";
      return false;
    }
  }
  fs::rename(temporary, file_, ec);
  if (ec) {
    error = L"Nastavení nelze uložit do „" + file_.wstring() + L"“.";
    return false;
  }
  return true;
}
