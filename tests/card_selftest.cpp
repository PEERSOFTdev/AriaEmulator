// Folder cards without any firmware (HANDOFF 6.8, ar-aj1): a host folder
// becomes an SRAM card image and the card's changes go back into the
// folder. What the Aria would do to the card is played by a second folder
// whose card is saved into the first one's home. Prints PASS on success.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "card_folder.h"
#include "fat12.h"
#include "settings.h"
#include "text_codec.h"

namespace fs = std::filesystem;

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::cout << "FAIL " << what << "\n";
    ++failures;
  }
}

void Put(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << text;
}

std::string Get(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string Card(TextEncoding encoding, const std::wstring& name) {
  const std::vector<uint8_t> bytes = Encode(encoding, name);
  return std::string(bytes.begin(), bytes.end());
}

bool Listed(const Fat12Volume& volume, const std::string& directory, const std::string& name) {
  std::vector<FatEntry> entries;
  if (!volume.List(directory, entries)) return false;
  return std::any_of(entries.begin(), entries.end(), [&](const FatEntry& e) { return e.name == name; });
}

int Attributes(const Fat12Volume& volume, const std::string& directory, const std::string& name) {
  std::vector<FatEntry> entries;
  if (!volume.List(directory, entries)) return -1;
  for (const FatEntry& e : entries) {
    if (e.name == name) return e.attributes;
  }
  return -1;
}

// Read-only files would stop remove_all.
void RemoveAll(const fs::path& root) {
  std::error_code ec;
  for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
    SetFileAttributesW(it->path().c_str(), it->is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL);
  }
  fs::remove_all(root, ec);
}

std::string Read(const Fat12Volume& volume, const std::string& path) {
  std::vector<uint8_t> data;
  if (!volume.ReadFile(path, data)) return "<missing>";
  return std::string(data.begin(), data.end());
}

}  // namespace

int main() {
  const fs::path root = fs::temp_directory_path() / ("aria_card_selftest_" + std::to_string(GetCurrentProcessId()));
  RemoveAll(root);
  const fs::path a = root / L"Moje karta";
  const fs::path b = root / L"Aria";

  // The folder as the user fills it.
  Put(a / L"Dopis.txt", "Mila Ario,\r\n");
  Put(a / L"Žluťoučký kůň.txt", "kun\r\n");
  Put(a / L"longname1.txt", "1");
  Put(a / L"longname2.txt", "2");
  Put(a / L"prazdny", "");
  Put(a / L"Podzložka" / L"x.txt", "x\r\n");
  std::string big(70000, 'B');
  Put(a / L"Podzložka" / L"velky.bin", big);
  Put(a / L"skryty.txt", "s");
  SetFileAttributesW((a / L"skryty.txt").c_str(),
                     FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE);

  for (TextEncoding encoding : {TextEncoding::kCp852, TextEncoding::kKamenicky}) {
    FolderCard card;
    std::vector<uint8_t> image;
    std::wstring error;
    Check(card.Load(a, 512 * 1024, encoding, image, error), "a folder loads into a 512 KB card");
    Check(image.size() == 512 * 1024, "the image has the card's size");
    Fat12Volume volume;
    Check(volume.Open(image, error), "the image is a FAT12 volume");
    Check(volume.Label() == "MOJE KARTA", "the folder's name is the volume label, space kept");
    Check(Read(volume, "DOPIS.TXT") == "Mila Ario,\r\n", "a plain name keeps its 8.3 form and contents");
    Check(Listed(volume, "", Card(encoding, L"ŽLUŤOU~1.TXT")), "a Czech long name in the code page, with ~1");
    Check(Listed(volume, "", "LONGNA~1.TXT") && Listed(volume, "", "LONGNA~2.TXT"), "clashing names get ~1, ~2");
    Check(Listed(volume, "", "PRAZDNY") && Read(volume, "PRAZDNY").empty(), "an empty file without extension");
    const std::string sub = Card(encoding, L"PODZLO~1");
    Check(Read(volume, sub + "\\X.TXT") == "x\r\n", "a file in a subfolder");
    Check(Read(volume, sub + "\\VELKY.BIN") == big, "a file across many clusters");
    Check(Attributes(volume, "", "SKRYTY.TXT") == 0x23, "hidden, read-only and archive go onto the card");
    Check(Attributes(volume, "", "DOPIS.TXT") == 0x20, "a plain file has archive only");
  }

  // The card the Aria hands back: Dopis changed, Podzložka\x.txt kept,
  // longname1 kept, a new file and a new folder, everything else deleted.
  Put(b / L"DOPIS.TXT", "Mila Ario, dekuji.\r\n");
  Put(b / L"PODZLO~1" / L"X.TXT", "x\r\n");
  Put(b / L"LONGNA~1.TXT", "1");
  Put(b / L"NOVY.TXT", "novy\r\n");
  Put(b / L"NOVA" / L"Y.TXT", "y\r\n");
  // Attributes the Aria set or cleared without touching the contents:
  // skryty.txt is plain again, longname1 read-only, the new folder hidden.
  Put(b / L"SKRYTY.TXT", "s");
  SetFileAttributesW((b / L"LONGNA~1.TXT").c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE);
  SetFileAttributesW((b / L"NOVA").c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY);
  // A hidden file the Aria rewrites: Windows will not simply overwrite it.
  SetFileAttributesW((a / L"Dopis.txt").c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE);
  {
    FolderCard home, aria;
    std::vector<uint8_t> image, changed;
    std::wstring error;
    Check(home.Load(a, 512 * 1024, TextEncoding::kCp852, image, error), "the home folder loads");
    Check(aria.Load(b, 512 * 1024, TextEncoding::kCp852, changed, error), "the changed card builds");
    Check(home.Save(changed, error), "the changed card saves into the home folder");
    Check(Get(a / L"Dopis.txt") == "Mila Ario, dekuji.\r\n", "a changed file keeps its host name");
    Check(Get(a / L"Podzložka" / L"x.txt") == "x\r\n", "a kept file in a kept subfolder stays");
    Check(Get(a / L"longname1.txt") == "1", "a kept file under a ~1 name keeps its long name");
    Check(Get(a / L"NOVY.TXT") == "novy\r\n", "a new file appears under its 8.3 name");
    Check(Get(a / L"NOVA" / L"Y.TXT") == "y\r\n", "a new folder appears with its file");
    Check(!fs::exists(a / L"longname2.txt") && Get(a / FolderCard::kTrash / L"longname2.txt") == "2",
          "a deleted file goes to the trash");
    Check(Get(a / FolderCard::kTrash / L"Žluťoučký kůň.txt") == "kun\r\n", "a deleted Czech name keeps its name there");
    Check(Get(a / FolderCard::kTrash / L"Podzložka" / L"velky.bin") == big,
          "a deleted file in a subfolder keeps its place in the trash");
    Check(fs::exists(a / L"Podzložka"), "a folder that still has files stays");
    auto host = [](const fs::path& path) { return GetFileAttributesW(path.c_str()) & 0x27; };
    Check(host(a / L"skryty.txt") == FILE_ATTRIBUTE_ARCHIVE, "attributes cleared on the card are cleared in the folder");
    Check(host(a / L"longname1.txt") == (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE),
          "read-only set on the card is set in the folder");
    Check(GetFileAttributesW((a / L"NOVA").c_str()) & FILE_ATTRIBUTE_HIDDEN, "a hidden folder stays hidden");
    Check(host(a / L"Dopis.txt") == FILE_ATTRIBUTE_ARCHIVE, "a hidden file is rewritten and takes the card's attributes");

    // Saving the same card again changes nothing and loses nothing.
    Check(home.Save(changed, error), "a second save");
    Check(Get(a / L"Dopis.txt") == "Mila Ario, dekuji.\r\n", "the second save keeps the change");

    // The trash never goes onto the card.
    FolderCard again;
    std::vector<uint8_t> reloaded;
    Check(again.Load(a, 512 * 1024, TextEncoding::kCp852, reloaded, error), "the saved folder loads again");
    Fat12Volume volume;
    Check(volume.Open(reloaded, error) && !Listed(volume, "", ".ARIA-TR") && !Listed(volume, "", "_ARIA-~1"),
          "the trash is not on the card");
  }

  // A label by the Aria's rules (README.DOC): forbidden characters dropped,
  // spaces kept once, eleven characters in the code page.
  for (TextEncoding encoding : {TextEncoding::kCp852, TextEncoding::kKamenicky}) {
    Put(root / L"Dopisy (stará) & spol." / L"a.txt", "a");
    FolderCard card;
    std::vector<uint8_t> image;
    std::wstring error;
    Fat12Volume volume;
    Check(card.Load(root / L"Dopisy (stará) & spol.", 64 * 1024, encoding, image, error) &&
              volume.Open(image, error) && volume.Label() == Card(encoding, L"DOPISY STAR"),
          "a label without ( ) & . and cut to eleven characters");
  }

  // Too big: the user hears what would fit.
  {
    Put(root / L"Velka" / L"data.bin", std::string(600 * 1024, 'D'));
    FolderCard card;
    std::vector<uint8_t> image;
    std::wstring error;
    uint32_t needed = 0;
    Check(!card.Load(root / L"Velka", 512 * 1024, TextEncoding::kCp852, image, error, &needed),
          "a folder bigger than the card is refused");
    Check(needed == 1024 * 1024, "and the smallest card that fits is named");
    Check(error.find(L"1024 KB") != std::wstring::npos, "in the message too");
    std::vector<uint32_t> sizes;
    uint64_t bytes = 0;
    Check(FolderCard::FittingSizes(root / L"Velka", TextEncoding::kCp852, sizes, bytes, error) &&
              bytes == 600 * 1024 && !sizes.empty() && sizes.front() == 1024 * 1024 &&
              sizes.back() == FolderCard::kMaxSize && sizes.size() == 7,
          "the sizes offered for it are 1 MB to 64 MB");
  }

  // A file the card cannot read (a directory entry claiming 3.9 MB over a
  // 1-cluster chain, as the user saw SYMDEB.EXE) stops nothing else: the
  // new file arrives, the damaged one is named, its folder copy stays and
  // is not taken for deleted.
  {
    const fs::path home = root / L"Poskozena";
    const fs::path aria = root / L"PoskozenaAria";
    Put(home / L"a.txt", "a");
    Put(home / L"b.txt", "b");
    Put(aria / L"A.TXT", "a");
    Put(aria / L"B.TXT", "b");
    Put(aria / L"POKUS.TXT", "pokus\r\n");
    FolderCard card, other;
    std::vector<uint8_t> image, changed;
    std::wstring error;
    Check(card.Load(home, 1024 * 1024, TextEncoding::kCp852, image, error) &&
              other.Load(aria, 1024 * 1024, TextEncoding::kCp852, changed, error),
          "the cards for the damage test build");
    const uint32_t root_offset = (changed[14] | changed[15] << 8) * 512u +
                                 changed[16] * (changed[22] | changed[23] << 8) * 512u;
    bool hit = false;
    for (uint32_t at = root_offset; at < root_offset + 512 * 32; at += 32) {
      if (std::string(reinterpret_cast<const char*>(&changed[at]), 11) == "A       TXT") {
        const uint32_t size = 0x003E0000;
        for (int i = 0; i < 4; ++i) changed[at + 28 + i] = static_cast<uint8_t>(size >> (8 * i));
        hit = true;
      }
    }
    std::vector<std::wstring> damaged;
    Check(hit && card.Save(changed, error, &damaged), "a card with a damaged file still saves");
    Check(damaged.size() == 1 && damaged.front() == L"A.TXT", "the damaged file is named");
    Check(Get(home / L"a.txt") == "a" && !fs::exists(home / FolderCard::kTrash / L"a.txt"),
          "its folder copy stays and is not trashed");
    Check(Get(home / L"POKUS.TXT") == "pokus\r\n", "the new file arrives anyway");
  }

  // What the emulator remembers about cards survives a save and a load,
  // diacritics and a ';' in the path included, and a folder typed another
  // way is the same card.
  {
    const fs::path file = root / L"nastaveni.txt";
    const std::wstring folder = (root / L"Dopisy; staré").wstring();
    Settings saved(file);
    saved.RememberCard(folder, 2 * 1024 * 1024, true);
    saved.RememberCard((root / L"Jiná").wstring(), 512 * 1024, false);
    saved.SetLastCard(folder);
    std::wstring error;
    Check(saved.Save(error), "the settings save");
    Settings loaded(file);
    loaded.Load();
    const Settings::Card* card = loaded.FindCard(folder + L"\\");
    Check(card && card->size == 2 * 1024 * 1024 && card->locked, "a card's size and lock come back");
    Check(loaded.last_card() == folder, "the last card comes back");
    const Settings::Card* other = loaded.FindCard((root / L"JINÁ").wstring());
    Check(other && other->size == 512 * 1024 && !other->locked, "folders compare without case");
    Check(!loaded.FindCard((root / L"Nikdy").wstring()), "an unknown folder is no card");
  }

  RemoveAll(root);
  if (failures == 0) std::cout << "PASS card_selftest\n";
  return failures == 0 ? 0 : 1;
}
