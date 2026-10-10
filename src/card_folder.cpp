#include "card_folder.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>

#include "fat12.h"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kSector = 512;
// Root directory entries of a folder card: DLFMT's own maximum (DLFMT.EXE
// usage text: "Default is 240, Max: 512"), so a folder of many loose files
// still fits; subdirectories have no limit of their own.
constexpr uint16_t kRootEntries = 512;

uint64_t Hash(const std::vector<uint8_t>& data) {
  uint64_t hash = 14695981039346656037ull;  // FNV-1a.
  for (uint8_t byte : data) hash = (hash ^ byte) * 1099511628211ull;
  return hash;
}

bool ReadHostFile(const fs::path& path, std::vector<uint8_t>& data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return !file.bad();
}

// DOS date and time of a host file's last write, in local time as DOS keeps
// it; 1.1.1980 when Windows cannot say.
void HostTime(const fs::path& path, uint16_t& time, uint16_t& date) {
  time = 0;
  date = (0 << 9) | (1 << 5) | 1;
  WIN32_FILE_ATTRIBUTE_DATA info;
  FILETIME local;
  WORD dos_date = 0, dos_time = 0;
  if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info) &&
      FileTimeToLocalFileTime(&info.ftLastWriteTime, &local) && FileTimeToDosDateTime(&local, &dos_date, &dos_time)) {
    time = dos_time;
    date = dos_date;
  }
}

void SetHostTime(const fs::path& path, uint16_t time, uint16_t date) {
  FILETIME local, utc;
  if (!DosDateTimeToFileTime(date, time, &local) || !LocalFileTimeToFileTime(&local, &utc)) return;
  HANDLE file = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (file == INVALID_HANDLE_VALUE) return;
  SetFileTime(file, nullptr, nullptr, &utc);
  CloseHandle(file);
}

// One part of an 8.3 name in the card's encoding: upper case, spaces
// dropped, what DOS does not take as '_'. `exact` turns false when anything
// but the case changed.
std::string NamePart(const std::wstring& source, TextEncoding encoding, bool& exact) {
  std::wstring upper = source;
  if (!upper.empty()) CharUpperBuffW(upper.data(), static_cast<DWORD>(upper.size()));
  std::string out;
  for (wchar_t ch : upper) {
    if (ch == L' ') {
      exact = false;
      continue;
    }
    const std::vector<uint8_t> bytes = Encode(encoding, std::wstring_view(&ch, 1));
    for (uint8_t byte : bytes) {
      // Encode marks a character the code page lacks with '?'; it also
      // folds accents it cannot keep, which is not exact either.
      const bool bad = byte < 0x20 || std::strchr("\"*+,./:;<=>?[\\]|", byte) != nullptr;
      if (bad || Decode(encoding, &byte, 1) != std::wstring(1, ch)) exact = false;
      out.push_back(bad ? '_' : static_cast<char>(byte));
    }
  }
  return out;
}

// The 11-byte directory name of `host` within a directory whose names so far
// are `taken`.
std::string ShortName(const std::wstring& host, TextEncoding encoding, const std::set<std::string>& taken) {
  std::wstring stem = host, extension;
  const std::size_t dot = host.find_last_of(L'.');
  if (dot != std::wstring::npos && dot > 0) {
    stem = host.substr(0, dot);
    extension = host.substr(dot + 1);
  }
  bool exact = true;
  std::string base = NamePart(stem, encoding, exact);
  std::string ext = NamePart(extension, encoding, exact);
  if (base.empty()) {
    base = "_";
    exact = false;
  }
  if (base.size() > 8 || ext.size() > 3) exact = false;
  if (ext.size() > 3) ext.resize(3);
  auto pack = [](std::string name, const std::string& ext) {
    name.resize(8, ' ');
    std::string padded = ext;
    padded.resize(3, ' ');
    return name + padded;
  };
  std::string name;
  if (exact && !taken.count(name = pack(base, ext))) return name;
  for (int n = 1;; ++n) {
    const std::string tail = "~" + std::to_string(n);
    name = pack(base.substr(0, 8 - tail.size()) + tail, ext);
    if (!taken.count(name)) return name;
  }
}

// "NAME    EXT" -> "NAME.EXT", as Fat12Volume names entries.
std::string Dotted(const std::string& packed) {
  std::string name = packed.substr(0, 8), ext = packed.substr(8, 3);
  while (!name.empty() && name.back() == ' ') name.pop_back();
  while (!ext.empty() && ext.back() == ' ') ext.pop_back();
  return ext.empty() ? name : name + "." + ext;
}

struct Node {
  fs::path host;
  std::string name;  // 11 bytes, as in the directory entry.
  bool directory = false;
  uint64_t size = 0;
  uint16_t time = 0, date = 0;
  bool read_only = false;
  std::vector<Node> children;
};

bool Scan(const fs::path& folder, TextEncoding encoding, bool top, Node& node, std::wstring& error) {
  std::vector<fs::directory_entry> entries;
  std::error_code ec;
  for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
    if (top && it->path().filename() == FolderCard::kTrash) continue;
    entries.push_back(*it);
  }
  if (ec) {
    error = L"Složku „" + folder.wstring() + L"“ nelze přečíst: " + std::wstring(ec.message().begin(), ec.message().end());
    return false;
  }
  std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
    return lstrcmpiW(a.path().filename().c_str(), b.path().filename().c_str()) < 0;
  });
  std::set<std::string> taken;
  for (const fs::directory_entry& entry : entries) {
    Node child;
    child.host = entry.path();
    child.directory = entry.is_directory(ec);
    child.name = ShortName(entry.path().filename().wstring(), encoding, taken);
    taken.insert(child.name);
    HostTime(entry.path(), child.time, child.date);
    const DWORD attributes = GetFileAttributesW(entry.path().c_str());
    child.read_only = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY);
    if (child.directory) {
      if (!Scan(entry.path(), encoding, false, child, error)) return false;
    } else {
      child.size = entry.file_size(ec);
      if (ec || child.size > FolderCard::kMaxSize) {
        error = L"Soubor „" + entry.path().wstring() + L"“ nelze přečíst, nebo je větší než největší karta.";
        return false;
      }
    }
    node.children.push_back(std::move(child));
  }
  return true;
}

// Geometry of a volume FormatFat12 made.
struct Layout {
  uint32_t cluster_bytes = 0;
  uint32_t fat_offset = 0, fat_bytes = 0;
  uint32_t root_offset = 0, data_offset = 0;
  uint32_t clusters = 0;
};

Layout LayoutOf(const std::vector<uint8_t>& image) {
  const uint8_t* boot = image.data();
  auto word = [&](int at) { return static_cast<uint32_t>(boot[at] | boot[at + 1] << 8); };
  Layout layout;
  layout.cluster_bytes = boot[13] * kSector;
  layout.fat_offset = word(14) * kSector;
  layout.fat_bytes = word(22) * kSector;
  layout.root_offset = layout.fat_offset + boot[16] * layout.fat_bytes;
  layout.data_offset = layout.root_offset + word(17) * 32;
  layout.clusters = static_cast<uint32_t>((image.size() - layout.data_offset) / layout.cluster_bytes);
  return layout;
}

uint32_t ClustersFor(uint64_t bytes, uint32_t cluster_bytes) {
  return static_cast<uint32_t>((bytes + cluster_bytes - 1) / cluster_bytes);
}

// Clusters a directory's contents take, the directory's own included
// (root's entries live in the root area and cost none).
uint64_t ClustersOf(const Node& node, uint32_t cluster_bytes, bool root) {
  uint64_t total = root ? 0 : ClustersFor((node.children.size() + 2) * 32u, cluster_bytes);
  for (const Node& child : node.children) {
    total += child.directory ? ClustersOf(child, cluster_bytes, false) : ClustersFor(child.size, cluster_bytes);
  }
  return total;
}

Fat12Format CardFormat(uint32_t size, const std::wstring& label, TextEncoding encoding) {
  Fat12Format format;
  format.sectors = size / kSector;
  format.root_entries = kRootEntries;
  bool ignored = true;
  format.label = NamePart(label, encoding, ignored).substr(0, 11);
  return format;
}

bool Fits(const Node& root, uint32_t size) {
  if (root.children.size() + 1 > kRootEntries) return false;  // + the label.
  const std::vector<uint8_t> image = FormatFat12(CardFormat(size, L"", TextEncoding::kCp852));
  const Layout layout = LayoutOf(image);
  return ClustersOf(root, layout.cluster_bytes, true) <= layout.clusters;
}

class Writer {
 public:
  Writer(std::vector<uint8_t>& image, std::map<std::string, std::pair<fs::path, uint64_t>>& files)
      : image_(image), layout_(LayoutOf(image)), files_(files) {}

  bool WriteRoot(const Node& root, std::wstring& error) {
    std::vector<uint8_t> records;
    if (!Children(root, "", 0, records, error)) return false;
    // Entry 0 holds the volume label FormatFat12 wrote.
    std::memcpy(image_.data() + layout_.root_offset + 32, records.data(), records.size());
    return true;
  }

 private:
  uint16_t Allocate(uint32_t count) {
    if (count == 0) return 0;
    const uint16_t first = static_cast<uint16_t>(next_);
    for (uint32_t i = 0; i < count; ++i) SetFat(next_ + i, i + 1 < count ? next_ + i + 1 : 0xFFF);
    next_ += count;
    return first;
  }

  void SetFat(uint32_t cluster, uint32_t value) {
    for (uint32_t copy = 0; copy < 2; ++copy) {
      uint8_t* p = image_.data() + layout_.fat_offset + copy * layout_.fat_bytes + cluster * 3 / 2;
      if (cluster & 1) {
        p[0] = static_cast<uint8_t>((p[0] & 0x0F) | (value << 4));
        p[1] = static_cast<uint8_t>(value >> 4);
      } else {
        p[0] = static_cast<uint8_t>(value);
        p[1] = static_cast<uint8_t>((p[1] & 0xF0) | ((value >> 8) & 0x0F));
      }
    }
  }

  uint8_t* Cluster(uint16_t cluster) {
    return image_.data() + layout_.data_offset + static_cast<std::size_t>(cluster - 2) * layout_.cluster_bytes;
  }

  static void Record(std::vector<uint8_t>& out, const std::string& name, uint8_t attributes, uint16_t time,
                     uint16_t date, uint16_t cluster, uint32_t size) {
    uint8_t entry[32] = {};
    std::memcpy(entry, name.data(), 11);
    if (entry[0] == 0xE5) entry[0] = 0x05;  // 0E5h marks a deleted entry.
    entry[11] = attributes;
    entry[22] = static_cast<uint8_t>(time);
    entry[23] = static_cast<uint8_t>(time >> 8);
    entry[24] = static_cast<uint8_t>(date);
    entry[25] = static_cast<uint8_t>(date >> 8);
    entry[26] = static_cast<uint8_t>(cluster);
    entry[27] = static_cast<uint8_t>(cluster >> 8);
    for (int i = 0; i < 4; ++i) entry[28 + i] = static_cast<uint8_t>(size >> (8 * i));
    out.insert(out.end(), entry, entry + 32);
  }

  // Lays out `node`'s children and returns their directory records.
  bool Children(const Node& node, const std::string& path, uint16_t own, std::vector<uint8_t>& records,
                std::wstring& error) {
    for (const Node& child : node.children) {
      const std::string card_path = path.empty() ? Dotted(child.name) : path + "\\" + Dotted(child.name);
      if (child.directory) {
        const uint32_t count = ClustersFor((child.children.size() + 2) * 32u, layout_.cluster_bytes);
        const uint16_t first = Allocate(count);
        std::vector<uint8_t> inner;
        Record(inner, ".          ", 0x10, child.time, child.date, first, 0);
        Record(inner, "..         ", 0x10, child.time, child.date, own, 0);
        if (!Children(child, card_path, first, inner, error)) return false;
        for (uint32_t i = 0; i < count; ++i) {
          const std::size_t from = static_cast<std::size_t>(i) * layout_.cluster_bytes;
          const std::size_t length = std::min<std::size_t>(layout_.cluster_bytes, inner.size() > from ? inner.size() - from : 0);
          if (length) std::memcpy(Cluster(static_cast<uint16_t>(first + i)), inner.data() + from, length);
        }
        files_[card_path] = {child.host, 0};
        Record(records, child.name, 0x10, child.time, child.date, first, 0);
      } else {
        std::vector<uint8_t> data;
        if (!ReadHostFile(child.host, data)) {
          error = L"Soubor „" + child.host.wstring() + L"“ nelze přečíst.";
          return false;
        }
        const uint16_t first = Allocate(ClustersFor(data.size(), layout_.cluster_bytes));
        if (!data.empty()) std::memcpy(Cluster(first), data.data(), data.size());
        files_[card_path] = {child.host, Hash(data)};
        Record(records, child.name, child.read_only ? 0x21 : 0x20, child.time, child.date, first,
               static_cast<uint32_t>(data.size()));
      }
    }
    return true;
  }

  std::vector<uint8_t>& image_;
  Layout layout_;
  std::map<std::string, std::pair<fs::path, uint64_t>>& files_;
  uint32_t next_ = 2;
};

// A free name for `path` inside the trash: the same, or with " (2)", " (3)"...
fs::path TrashPlace(const fs::path& path) {
  if (!fs::exists(path)) return path;
  for (int n = 2;; ++n) {
    fs::path candidate = path.parent_path() / (path.stem().wstring() + L" (" + std::to_wstring(n) + L")" +
                                               path.extension().wstring());
    if (!fs::exists(candidate)) return candidate;
  }
}

}  // namespace

bool FolderCard::Load(const fs::path& folder, uint32_t size, TextEncoding encoding, std::vector<uint8_t>& image,
                      std::wstring& error, uint32_t* needed) {
  if (needed) *needed = 0;
  Node root;
  if (!Scan(folder, encoding, true, root, error)) return false;
  if (!Fits(root, size)) {
    uint32_t fitting = 0;
    for (uint32_t candidate = kMinSize; candidate <= kMaxSize && !fitting; candidate *= 2) {
      if (Fits(root, candidate)) fitting = candidate;
    }
    if (needed) *needed = fitting;
    error = L"Složka „" + folder.wstring() + L"“ se na kartu " + std::to_wstring(size / 1024) + L" KB nevejde";
    error += fitting ? L"; stačila by karta " + std::to_wstring(fitting / 1024) + L" KB."
                     : root.children.size() + 1 > kRootEntries
                           ? L": má víc než " + std::to_wstring(kRootEntries - 1) + L" položek v kořeni."
                           : L" ani na největší kartu (64 MB).";
    return false;
  }
  image = FormatFat12(CardFormat(size, folder.filename().wstring(), encoding));
  std::map<std::string, std::pair<fs::path, uint64_t>> files;
  Writer writer(image, files);
  if (!writer.WriteRoot(root, error)) return false;
  home_ = folder;
  encoding_ = encoding;
  imported_.clear();
  for (const auto& [path, host] : files) {
    Imported item;
    item.host = host.first;
    item.hash = host.second;
    item.directory = fs::is_directory(host.first);
    imported_[path] = item;
  }
  return true;
}

bool FolderCard::Save(const std::vector<uint8_t>& image, std::wstring& error) {
  Fat12Volume volume;
  if (!volume.Open(image, error)) {
    error = L"Kartu nelze zapsat do složky „" + home_.wstring() + L"“: " + error;
    return false;
  }
  std::map<std::string, Imported> now;
  std::error_code ec;
  // Breadth-first over the card's directories, parents before children.
  std::vector<std::pair<std::string, fs::path>> pending = {{"", home_}};
  for (std::size_t at = 0; at < pending.size(); ++at) {
    const auto [directory, host_directory] = pending[at];
    std::vector<FatEntry> entries;
    if (!volume.List(directory, entries)) {
      error = L"Adresář karty je poškozený, složka „" + home_.wstring() + L"“ zůstala, jak byla.";
      return false;
    }
    for (const FatEntry& entry : entries) {
      const std::string path = directory.empty() ? entry.name : directory + "\\" + entry.name;
      std::string raw = entry.name;
      if (!raw.empty() && static_cast<uint8_t>(raw[0]) == 0x05) raw[0] = static_cast<char>(0xE5);
      const auto known = imported_.find(path);
      Imported item;
      item.directory = entry.IsDirectory();
      item.host = known != imported_.end() && known->second.directory == item.directory
                      ? known->second.host
                      : host_directory / Decode(encoding_, reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
      if (item.directory) {
        fs::create_directories(item.host, ec);
        if (ec) {
          error = L"Nelze vytvořit složku „" + item.host.wstring() + L"“.";
          return false;
        }
        pending.emplace_back(path, item.host);
        now[path] = item;
        continue;
      }
      std::vector<uint8_t> data;
      if (!volume.ReadFile(path, data)) {
        error = L"Soubor " + Decode(encoding_, reinterpret_cast<const uint8_t*>(path.data()), path.size()) +
                L" na kartě je poškozený, složka „" + home_.wstring() + L"“ ho nedostala.";
        return false;
      }
      item.hash = Hash(data);
      now[path] = item;
      if (known != imported_.end() && !known->second.directory && known->second.hash == item.hash &&
          fs::exists(item.host)) {
        continue;
      }
      // The Aria changed it, so it was not read-only on the card.
      if (fs::exists(item.host)) fs::permissions(item.host, fs::perms::owner_write, fs::perm_options::add, ec);
      std::ofstream file(item.host, std::ios::binary | std::ios::trunc);
      file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
      file.close();
      if (!file) {
        error = L"Soubor „" + item.host.wstring() + L"“ nelze zapsat.";
        return false;
      }
      SetHostTime(item.host, entry.time, entry.date);
    }
  }
  // What the Aria deleted goes to the trash, files first; a directory goes
  // only once it is empty, so nothing the card never had is lost with it.
  std::vector<std::pair<std::string, Imported>> gone;
  for (const auto& [path, item] : imported_) {
    if (!now.count(path)) gone.emplace_back(path, item);
  }
  for (const auto& [path, item] : gone) {
    if (item.directory || !fs::exists(item.host)) continue;
    const fs::path place = TrashPlace(home_ / kTrash / fs::relative(item.host, home_, ec));
    fs::create_directories(place.parent_path(), ec);
    fs::rename(item.host, place, ec);
    if (ec) {
      error = L"Soubor „" + item.host.wstring() + L"“, který Aria smazala, nelze přesunout do " + kTrash + L".";
      return false;
    }
  }
  std::sort(gone.begin(), gone.end(), [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
  for (const auto& [path, item] : gone) {
    if (item.directory) fs::remove(item.host, ec);  // Only when empty.
  }
  imported_ = std::move(now);
  return true;
}
