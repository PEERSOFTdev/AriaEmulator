#include "disk_drive.h"

#include "floppy_image.h"
#include "res/resource.h"
#include "win/dialog.h"

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kHighDensity = 1474560;

}  // namespace

DiskDrive::DiskDrive(EmulatorThread& emulator, Settings& settings, TextEncoding encoding)
    : Drive(emulator, settings, encoding) {
  texts_.caption = L"Disketa";
  texts_.nominative = L"disketa";
  texts_.accusative = L"disketu";
  texts_.locative = L"na disketě";
  texts_.onto = L"na disketu";
  texts_.without = L"bez diskety";
  texts_.inside = L"v mechanice";
  texts_.menu = L"Disketa";
  texts_.image_title = L"Vyberte obraz diskety";
  texts_.image_filter = L"Obrazy disket";
  texts_.image_pattern = floppy_image::kPattern;
  texts_.damaged_dump = L"disketa-poskozena.img";
  texts_.slots.caption = L"Sloty s disketami";
  texts_.slots.current = L"Sem &vloženou disketu";
  texts_.slots.lock = L"&Zamknout tuto disketu proti zápisu";
  texts_.slots.folder_title = L"Vyberte složku s disketou pro tento slot";
  texts_.slots.image_title = L"Vyberte obraz diskety pro tento slot";
  texts_.slots.image_filter = L"Obrazy disket";
  texts_.slots.image_pattern = floppy_image::kPattern;
}

void DiskDrive::Insert(AriaMachine& machine, std::vector<uint8_t> image, bool locked) {
  // The image was read and padded by floppy_image::Read, so it is whole.
  std::optional<Fdc82077::Disk> disk = Fdc82077::Disk::FromImage(std::move(image), locked);
  if (!disk) return;
  // The drive's disk-change line tells BACKPACK.SYS at once, so no pause
  // is needed between two diskettes (HANDOFF 6.7).
  machine.backpack().fdc().Eject();
  machine.backpack().fdc().Insert(std::move(*disk));
}

std::optional<std::vector<uint8_t>> DiskDrive::Take(AriaMachine& machine) {
  std::optional<Fdc82077::Disk> disk = machine.backpack().fdc().Eject();
  if (!disk) return std::nullopt;
  return std::move(disk->bytes);
}

bool DiskDrive::Writes(AriaMachine& machine, uint64_t& writes, bool& modified) {
  const Fdc82077::Disk* disk = machine.backpack().fdc().disk();
  if (!disk) return false;
  writes = disk->writes;
  modified = disk->modified;
  return true;
}

std::vector<uint8_t> DiskDrive::Contents(AriaMachine& machine) {
  const Fdc82077::Disk* disk = machine.backpack().fdc().disk();
  return disk ? disk->bytes : std::vector<uint8_t>{};
}

void DiskDrive::ClearModified(AriaMachine& machine, uint64_t writes) {
  if (Fdc82077::Disk* disk = machine.backpack().fdc().disk(); disk && disk->writes == writes) disk->modified = false;
}

void DiskDrive::SetProtected(AriaMachine& machine, bool locked) {
  if (Fdc82077::Disk* disk = machine.backpack().fdc().disk()) disk->write_protected = locked;
}

bool DiskDrive::ReadImage(const fs::path& file, std::vector<uint8_t>& image, bool& readOnly, std::wstring& error) {
  Fdc82077::Disk disk;
  if (!floppy_image::Read(file, disk, error)) return false;
  readOnly = disk.write_protected;
  image = std::move(disk.bytes);
  return true;
}

bool DiskDrive::WriteImage(const fs::path& file, const std::vector<uint8_t>& image, std::wstring& error) {
  return floppy_image::Write(file, image, error);
}

std::wstring DiskDrive::SizeText() const { return size_ == kHighDensity ? L"1,44 MB" : L"720 KB"; }

std::wstring DiskDrive::TitlePart() const {
  // Nothing while unplugged and empty: most users have no Backpack.
  if (!connected()) {
    return present() ? Drive::TitlePart() + L" (mechanika Backpack odpojená)" : std::wstring{};
  }
  return Drive::TitlePart();
}

void DiskDrive::InsertFromFolder() {
  const std::wstring folder = win::PickFolder(owner_, L"Vyberte složku, ze které bude disketa");
  if (folder.empty()) return;
  InsertFolderSlot(folder, KnownLocked(folder));
}

void DiskDrive::InsertFolderSlot(const std::wstring& folder, bool locked) {
  // Unlike a card, a diskette has no size to ask: always 1.44 MB.
  if (!MountFolder(folder, locked)) return;
  ToneInserted();
  if (locked_) ToneLocked();
}

bool DiskDrive::MountRememberedFolder(const std::wstring& folder, bool locked) {
  return MountFolder(folder, locked);
}

bool DiskDrive::MountFolder(const std::wstring& folder, bool locked) {
  if (!EjectIfIn(folder)) return false;
  auto disk = std::make_unique<FolderCard>();
  std::vector<uint8_t> image;
  std::wstring error;
  if (!disk->LoadDiskette(folder, encoding_, image, error)) {
    Tell(error, MB_ICONERROR);
    return false;
  }
  return PutIn(std::move(disk), folder, std::move(image), locked);
}

void DiskDrive::ConnectRemembered() {
  const bool on = connected();
  emulator_.Call([on](AriaMachine& machine) { machine.ConnectBackpack(on); });
}

void DiskDrive::ToggleConnection() {
  const bool on = !connected();
  settings_.SetBackpackConnected(on);
  emulator_.Call([on](AriaMachine& machine) { machine.ConnectBackpack(on); });
  std::wstring error;
  if (!settings_.Save(error)) Tell(error, MB_ICONWARNING);
  if (on) ToneInserted(); else ToneEjected();
  Changed();
  // BACKPACK.SYS looks for the drive at start-up only (HANDOFF 6.7).
  Tell(on ? L"Mechanika Backpack je připojená. Aria ji uvidí jako jednotku E: po teplém resetu (F11, Ctrl+P) "
            L"nebo po příštím zapnutí. Starý formát fleš karty se tím posune na další písmeno."
          : L"Mechanika Backpack je odpojená. Aria o ní přestane vědět po teplém resetu (F11, Ctrl+P) nebo po "
            L"příštím zapnutí; do té doby bude jednotka E: hlásit, že není připravená.",
       MB_ICONINFORMATION);
}

void DiskDrive::InsertNew() {
  NewDiskDialog dialog;
  if (dialog.ShowModal(owner_, IDD_DISK_NEW) != IDOK) return;
  if (dialog.kind() == NewDiskDialog::Kind::kFolder) {
    const std::wstring folder = win::PickFolderToCreate(owner_, L"Pojmenujte složku nové diskety", L"Disketa",
                                                        DialogFolder(/*image=*/false).c_str());
    if (folder.empty()) return;
    std::error_code ec;
    if (fs::exists(folder, ec) && (!fs::is_directory(folder, ec) || !fs::is_empty(folder, ec))) {
      Tell(L"„" + folder + L"“ už existuje a není to prázdná složka. Nová disketa potřebuje novou nebo prázdnou "
           L"složku; existující složku vložíte jako disketu přes F11, Ctrl+Shift+I.",
           MB_ICONERROR);
      return;
    }
    fs::create_directories(folder, ec);
    if (ec) {
      Tell(L"Složku „" + folder + L"“ nelze založit.", MB_ICONERROR);
      return;
    }
    settings_.SetDiskLocked(folder, false);
    if (!MountFolder(folder, /*locked=*/false)) return;
    ToneInserted();
    return;
  }
  const std::wstring file = win::PickFileToCreate(owner_, L"Pojmenujte obraz nové diskety", L"Obrazy disket",
                                                  floppy_image::kExtension, L"Disketa",
                                                  DialogFolder(/*image=*/true).c_str());
  if (file.empty()) return;
  // Writing over the image in the drive would lose it twice over: the
  // eject before the new diskette goes in would save the old one on top.
  if (present() && Settings::SamePath(home_.wstring(), file)) {
    Tell(L"Tento obraz je právě v mechanice. Novou disketu pojmenujte jinak, nebo starou nejdřív vysuňte.",
         MB_ICONERROR);
    return;
  }
  // Blank is F6h, what FORMAT leaves in a sector; the Aria's FORMAT asks
  // 720 KB or 1.44 MB and makes it either (HANDOFF 6.7).
  const std::vector<uint8_t> image =
      dialog.kind() == NewDiskDialog::Kind::kBlank
          ? std::vector<uint8_t>(kHighDensity, 0xF6)
          : FolderCard::FormattedDiskette(dialog.kind() == NewDiskDialog::Kind::kHigh,
                                          fs::path(file).stem().wstring(), encoding_);
  std::wstring error;
  if (!floppy_image::Write(file, image, error)) {
    Tell(error, MB_ICONERROR);
    return;
  }
  // A file written over keeps nothing of the diskette it was, its lock
  // included.
  settings_.SetDiskLocked(file, false);
  if (!MountImage(file, /*locked=*/false)) return;
  ToneInserted();
}
