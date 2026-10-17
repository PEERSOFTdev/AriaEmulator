// Folder cards without any firmware (HANDOFF 6.8, ar-aj1): a host folder
// becomes an SRAM card image and the card's changes go back into the
// folder. What the Aria would do to the card is played by a second folder
// whose card is saved into the first one's home. Also image cards and the
// settings that remember cards. Prints PASS on success.

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
#include "card_image.h"
#include "fdc82077.h"
#include "floppy_image.h"
#include "pc_card.h"
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
    saved.SetImageFolder((root / L"Obrazy karet").wstring());
    saved.SetSlot(1, folder);
    saved.SetSlot(9, (root / L"Obrazy karet" / L"Karta.img").wstring());
    saved.SetCardLocked((root / L"Jen zamčená").wstring(), true);
    saved.SetCardLocked(folder, true);
    std::wstring error;
    Check(saved.Save(error), "the settings save");
    Settings loaded(file);
    loaded.Load();
    const Settings::Card* card = loaded.FindCard(folder + L"\\");
    Check(card && card->size == 2 * 1024 * 1024 && card->locked, "a card's size and lock come back");
    Check(loaded.last_card() == folder, "the last card comes back");
    Check(loaded.image_folder() == (root / L"Obrazy karet").wstring(), "the image folder comes back");
    Check(loaded.slot(1) == folder && loaded.slot(9) == (root / L"Obrazy karet" / L"Karta.img").wstring() &&
              loaded.slot(2).empty(),
          "the slots come back");
    const Settings::Card* lockedOnly = loaded.FindCard((root / L"Jen zamčená").wstring());
    Check(lockedOnly && lockedOnly->size == 0 && lockedOnly->locked, "a lock without a size comes back");
    const Settings::Card* other = loaded.FindCard((root / L"JINÁ").wstring());
    Check(other && other->size == 512 * 1024 && !other->locked, "folders compare without case");
    Check(!loaded.FindCard((root / L"Nikdy").wstring()), "an unknown folder is no card");
    Check(!loaded.backpack_connected() && loaded.last_disk().empty(), "no Backpack unless connected");
    Check(!loaded.keyboard_connected(), "no external keyboard unless connected");
  }

  // The Backpack drive's settings (HANDOFF 6.7): the cable, the last
  // diskette, the image folder, locks and the nine diskette slots, apart
  // from the cards'.
  {
    const fs::path file = root / L"diskety.txt";
    const std::wstring image = (root / L"Diskety" / L"Hry; staré.img").wstring();
    Settings saved(file);
    saved.SetBackpackConnected(true);
    saved.SetKeyboardConnected(true);
    saved.SetLastDisk(image);
    saved.SetDiskFolder((root / L"Diskety").wstring());
    saved.SetDiskLocked(image, true);
    saved.SetDiskLocked((root / L"Odemčená.img").wstring(), true);
    saved.SetDiskLocked((root / L"Odemčená.img").wstring(), false);
    saved.SetDiskSlot(2, image);
    saved.SetSlot(2, (root / L"Karta").wstring());
    std::wstring error;
    Check(saved.Save(error), "the diskette settings save");
    Settings loaded(file);
    loaded.Load();
    Check(loaded.backpack_connected() && loaded.last_disk() == image &&
              loaded.disk_folder() == (root / L"Diskety").wstring(),
          "the cable, the last diskette and its folder come back");
    Check(loaded.DiskLocked(image) && !loaded.DiskLocked((root / L"Odemčená.img").wstring()),
          "a diskette's lock comes back, an unlocked one stays unlocked");
    Check(loaded.disk_slot(2) == image && loaded.slot(2) == (root / L"Karta").wstring() && loaded.disk_slot(1).empty(),
          "diskette slots are their own");
    Check(!loaded.FindCard(image), "a diskette is no card");
    Check(loaded.keyboard_connected(), "the external keyboard's adapter comes back");
  }

  // A settings file as an earlier build wrote it, or as a user edits it:
  // no BOM, a comment, spaces round the '=', and a line that makes no
  // sense; an image path is a card line like any other.
  {
    const fs::path file = root / L"rucne.txt";
    Put(file,
        "# ručně\r\n"
        "posledni-karta = D:\\Aria\\Karty\\Ariacard\r\n"
        "karta1=16777216;0;D:\\Aria\\Karty\\Ariacard\r\n"
        "karta2=nesmysl;1;D:\\X\r\n"
        "karta3=1048576;1;D:\\Aria\\Karty\\Pokus.img\r\n");
    Settings loaded(file);
    loaded.Load();
    const Settings::Card* folder = loaded.FindCard(L"d:/aria/karty/ariacard");
    Check(loaded.last_card() == L"D:\\Aria\\Karty\\Ariacard" && folder && folder->size == 16u << 20 &&
              !folder->locked,
          "an older settings file reads");
    Check(!loaded.FindCard(L"D:\\X"), "a broken card line is skipped");
    const Settings::Card* image = loaded.FindCard(L"D:\\Aria\\Karty\\Pokus.img");
    Check(image && image->locked, "an image card line reads");
  }

  // Image cards (card_image.h): a new formatted image is a FAT volume with
  // the Aria's label; an image goes to its file and back byte for byte,
  // over an older one too, with no temporary file left; a file of a size
  // no card has is refused.
  {
    const std::vector<uint8_t> formatted = FolderCard::Formatted(1u << 20, L"Nová karta", TextEncoding::kCp852);
    Fat12Volume volume;
    std::wstring error;
    std::vector<FatEntry> entries;
    Check(formatted.size() == 1u << 20 && volume.Open(formatted, error) && volume.Label() == "NOV\xb5 KARTA" &&
              volume.List("", entries) && entries.empty(),
          "a new image is formatted with the Aria's label");
    const fs::path file = root / L"Obrazy" / L"Karta.img";
    fs::create_directories(file.parent_path());
    std::vector<uint8_t> image(64 * 1024, 0xFF);
    Check(card_image::Write(file, image, error), "an image is written");
    image[100] = 0x42;
    Check(card_image::Write(file, image, error), "an image is written over");
    std::vector<uint8_t> back;
    Check(card_image::Read(file, back, error) && back == image, "an image reads back as written");
    Check(!fs::exists(file.wstring() + L".tmp"), "no temporary file stays");
    Put(root / L"Obrazy" / L"Spatna.img", std::string(100000, '\0'));
    Check(!card_image::Read(root / L"Obrazy" / L"Spatna.img", back, error) && !error.empty(),
          "an image of no card's size is refused");
    Check(card_image::ValidSize(64 * 1024) && card_image::ValidSize(64u << 20) && !card_image::ValidSize(32 * 1024) &&
              !card_image::ValidSize(128u << 20) && !card_image::ValidSize(3u << 20),
          "card sizes are powers of two from 64 KB to 64 MB");
    // A .flash image is a flash card, of whole 2 MB chip pairs.
    Check(card_image::IsFlash(L"a.flash") && card_image::IsFlash(L"B.FLASH") && !card_image::IsFlash(L"a.img") &&
              !card_image::IsFlash(L"flash"),
          "the .flash extension, in any case, makes a flash image");
    Check(card_image::ValidFlashSize(2u << 20) && card_image::ValidFlashSize(10u << 20) &&
              card_image::ValidFlashSize(64u << 20) && !card_image::ValidFlashSize(1u << 20) &&
              !card_image::ValidFlashSize(3u << 20) && !card_image::ValidFlashSize(66u << 20),
          "flash sizes are whole 2 MB pairs up to 64 MB");
    const fs::path flash_file = root / L"Obrazy" / L"Fleš.flash";
    Check(card_image::Write(flash_file, std::vector<uint8_t>(10u << 20, 0xFF), error) &&
              card_image::Read(flash_file, back, error) && back.size() == 10u << 20,
          "a 10 MB flash image reads");
    Put(root / L"Obrazy" / L"Spatna.flash", std::string(1u << 20, '\xFF'));
    Check(!card_image::Read(root / L"Obrazy" / L"Spatna.flash", back, error) && !error.empty(),
          "a 1 MB flash image is refused");
  }

  // Diskette images for the Backpack drive (fdc82077.h, HANDOFF 6.7): the
  // layout from the BPB, else from the size; short images padded with F6h.
  {
    auto bpb = [](size_t size, unsigned total, unsigned per_track) {
      std::vector<uint8_t> image(size);
      image[0x0B] = 0x00;
      image[0x0C] = 0x02;
      image[0x13] = static_cast<uint8_t>(total);
      image[0x14] = static_cast<uint8_t>(total >> 8);
      image[0x18] = static_cast<uint8_t>(per_track);
      image[0x1A] = 2;
      image[0x10] = 0x5A;  // A marker past the BPB, to see it kept.
      return image;
    };
    auto disk = Fdc82077::Disk::FromImage(bpb(1474560, 2880, 18), false);
    Check(disk && disk->sectors == 18 && disk->rate == 0 && disk->bytes.size() == 1474560, "a full 1.44 MB image");
    disk = Fdc82077::Disk::FromImage(bpb(9216, 2880, 18), true);
    Check(disk && disk->sectors == 18 && disk->bytes.size() == 1474560 && disk->bytes[9215] == 0 &&
              disk->bytes[9216] == 0xF6 && disk->bytes.back() == 0xF6 && disk->bytes[0x10] == 0x5A &&
              disk->write_protected,
          "a short 1.44 MB image by its BPB, padded with F6h");
    disk = Fdc82077::Disk::FromImage(bpb(9216, 1440, 9), false);
    Check(disk && disk->sectors == 9 && disk->rate == 2 && disk->bytes.size() == 737280, "a short 720 KB image by its BPB");
    disk = Fdc82077::Disk::FromImage(std::vector<uint8_t>(800 * 1024), false);
    Check(disk && disk->sectors == 18, "without a BPB, an image over 720 KB is a 1.44 MB disk");
    disk = Fdc82077::Disk::FromImage(std::vector<uint8_t>(100), false);
    Check(disk && disk->sectors == 9 && disk->bytes.size() == 737280, "without a BPB, a tiny image is a 720 KB disk");
    Check(!Fdc82077::Disk::FromImage(bpb(1474560, 2400, 15), false), "a 1.2 MB image is refused by its BPB");
    Check(!Fdc82077::Disk::FromImage(bpb(368640, 720, 9), false), "a 360 KB image is refused by its BPB");
    Check(!Fdc82077::Disk::FromImage(bpb(1474561, 2880, 18), false), "an image longer than its disk is refused");
    Check(!Fdc82077::Disk::FromImage(std::vector<uint8_t>(1474560 + 512), false), "an image over 1.44 MB is refused");
    Check(!Fdc82077::Disk::FromImage({}, false), "an empty file is refused");

    // New diskettes as DOS FORMAT lays them out, read back as FAT12.
    for (bool high : {true, false}) {
      const std::vector<uint8_t> image = FolderCard::FormattedDiskette(high, L"Nová", TextEncoding::kCp852);
      Fat12Volume volume;
      std::wstring error;
      std::vector<FatEntry> entries;
      const bool bpb = image.size() == (high ? 1474560u : 737280u) && image[0x0D] == (high ? 1 : 2) &&
                       image[0x11] == (high ? 224 : 112) && image[0x15] == (high ? 0xF0 : 0xF9) &&
                       image[0x16] == (high ? 9 : 3) && image[0x18] == (high ? 18 : 9) && image[0x1A] == 2;
      Check(bpb && volume.Open(image, error) && volume.List("", entries) && volume.Label() == "NOV\xb5" &&
                Fdc82077::Disk::FromImage(image, false),
            high ? "a new 1.44 MB diskette has FORMAT's BPB" : "a new 720 KB diskette has FORMAT's BPB");
    }
    // A file that may not be written is a diskette with its tab open.
    const fs::path locked_file = root / L"Zamčená.img";
    {
      const std::vector<uint8_t> image = FolderCard::FormattedDiskette(false, L"", TextEncoding::kCp852);
      std::ofstream out(locked_file, std::ios::binary);
      out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    }
    fs::permissions(locked_file, fs::perms::owner_write, fs::perm_options::remove);
    Fdc82077::Disk read_disk;
    std::wstring read_error;
    Check(floppy_image::Read(locked_file, read_disk, read_error) && read_disk.write_protected &&
              read_disk.sectors == 9,
          "a read-only image is a write-protected diskette");
    fs::permissions(locked_file, fs::perms::owner_write, fs::perm_options::add);
    Put(root / L"Velká.img", std::string(1474560 + 1, '\0'));
    Check(!floppy_image::Read(root / L"Velká.img", read_disk, read_error) && !read_error.empty(),
          "an image over 1.44 MB is refused with a reason");

    // A folder diskette: 1.44 MB as FORMAT lays it out, the folder's name
    // its label, its files on it; the drive takes it.
    const fs::path home = root / L"Disketa A";
    Put(home / L"hry.txt", "ahoj");
    FolderCard folder_disk;
    std::vector<uint8_t> image;
    Fat12Volume volume;
    std::vector<uint8_t> data;
    Check(folder_disk.LoadDiskette(home, TextEncoding::kCp852, image, read_error) && image.size() == 1474560 &&
              image[0x15] == 0xF0 && image[0x11] == 224 && Fdc82077::Disk::FromImage(image, false) &&
              volume.Open(image, read_error) && volume.Label() == "DISKETA A" && volume.ReadFile("HRY.TXT", data) &&
              std::string(data.begin(), data.end()) == "ahoj",
          "a folder is a 1.44 MB diskette with its files");
    // The Aria's FORMAT made it 720 KB: written back by the new BPB, the
    // file it no longer has goes to the trash.
    Check(folder_disk.Save(FolderCard::FormattedDiskette(false, L"", TextEncoding::kCp852), read_error) &&
              !fs::exists(home / L"hry.txt") && fs::exists(home / FolderCard::kTrash / L"hry.txt"),
          "a folder diskette formatted to 720 KB is written back, its old file in the trash");
    Put(root / L"Plná" / L"velky.bin", std::string(1500 * 1024, 'D'));
    FolderCard full;
    Check(!full.LoadDiskette(root / L"Plná", TextEncoding::kCp852, image, read_error) &&
              read_error.find(L"1500 KB") != std::wstring::npos,
          "a folder over 1.44 MB is refused with its size");
    for (int i = 0; i < 224; ++i) Put(root / L"Mnoho" / (std::to_wstring(i) + L".txt"), "x");
    FolderCard many;
    Check(!many.LoadDiskette(root / L"Mnoho", TextEncoding::kCp852, image, read_error) &&
              read_error.find(L"podsložky") != std::wstring::npos,
          "a folder with more root entries than a diskette has is refused");
  }

  // A flash card of two 28F008SA pairs (pc_card.h, HANDOFF 6.8): what
  // DLFFSA relies on, chip by chip. Even bytes are one chip, odd the other.
  {
    PcCard card = PcCard::Flash(4u << 20);
    Check(card.kind() == PcCard::Kind::kFlash && card.Size() == 4u << 20 && card.Read(0, false) == 0xFF,
          "a new flash card is erased");
    // CIS: CISTPL_DEVICE type 5 (flash), CISTPL_JEDEC_C 89h A2h, end.
    const uint8_t cis[] = {0x01, 0x03, 0x53, 0x00, 0xFF, 0x18, 0x02, 0x89, 0xA2, 0xFF};
    bool cis_ok = true;
    for (uint32_t i = 0; i < sizeof cis; ++i) {
      if (i != 3 && card.Read(2 * i, true) != cis[i]) cis_ok = false;
    }
    Check(cis_ok && card.Read(1, true) == 0xFF, "the flash CIS names flash and the chips' JEDEC codes");
    // The ID on the even chip of the first pair only.
    card.Write(0, false, 0x90);
    Check(card.Read(0, false) == 0x89 && card.Read(2, false) == 0xA2 && card.Read(1, false) == 0xFF,
          "90h shows Intel and 28F008SA on that chip alone");
    card.Write(0, false, 0xFF);
    // Programming only clears bits, and leaves the chip in status mode.
    card.Write(4, false, 0x40);
    card.Write(4, false, 0x5A);
    const uint8_t status = card.Read(4, false);
    card.Write(4, false, 0xFF);
    Check(status == 0x80 && card.Read(4, false) == 0x5A, "40h programs a byte and reports ready");
    card.Write(4, false, 0x40);
    card.Write(4, false, 0xA5);
    card.Write(4, false, 0xFF);
    Check(card.Read(4, false) == 0x00, "programming cannot set a bit");
    // Without the programming voltage: SR.3 and SR.4, nothing written,
    // until 50h clears them.
    card.Write(6, false, 0x40);
    card.Write(6, false, 0x00, /*vpp=*/false);
    const uint8_t no_vpp = card.Read(6, false);
    card.Write(6, false, 0x50);
    const uint8_t cleared = card.Read(6, false);
    card.Write(6, false, 0xFF);
    Check(no_vpp == 0x98 && cleared == 0x80 && card.Read(6, false) == 0xFF, "no Vpp fails programming with SR.3");
    // Erase: the 64 KB block of the even chip holding card address 20000h
    // is that chip's bytes 10000h-1FFFFh, card bytes 20000h-3FFFEh even.
    for (uint32_t at : {0x1FFFEu, 0x20000u, 0x3FFFEu, 0x40000u, 0x20001u}) {
      card.Write(at, false, 0x40);
      card.Write(at, false, 0x00);
    }
    card.Write(0x20000, false, 0x20);
    card.Write(0x20000, false, 0xD0);
    const uint8_t erased = card.Read(0x20000, false);
    card.Write(0x20000, false, 0xFF);
    card.Write(0x20001, false, 0xFF);
    Check(erased == 0x80 && card.Read(0x20000, false) == 0xFF && card.Read(0x3FFFE, false) == 0xFF &&
              card.Read(0x1FFFE, false) == 0x00 && card.Read(0x40000, false) == 0x00 &&
              card.Read(0x20001, false) == 0x00,
          "20h D0h erases one chip's block, every other byte");
    card.Write(8, false, 0x20);
    card.Write(8, false, 0xFF);
    const uint8_t sequence = card.Read(8, false);
    card.Write(8, false, 0x50);
    card.Write(8, false, 0xFF);
    Check(sequence == 0xB0, "20h without D0h is a command sequence error");
    // The second pair has chips of its own; past the last there are none,
    // and nothing mirrors.
    card.Write(2u << 20, false, 0x90);
    Check(card.Read(2u << 20, false) == 0x89 && card.Read(0, false) == 0xFF && card.Read(4u << 20, false) == 0xFF &&
              card.Read((4u << 20) + 4, false) == 0xFF,
          "each pair answers alone and nothing answers past the card");
    card.Write(2u << 20, false, 0xFF);
    // The write-protect switch stops every command.
    card.write_protected = true;
    card.Write(0, false, 0x90);
    Check(card.Read(0, false) == 0xFF, "a locked flash card takes no command");
  }

  RemoveAll(root);
  if (failures == 0) std::cout << "PASS card_selftest\n";
  return failures == 0 ? 0 : 1;
}
