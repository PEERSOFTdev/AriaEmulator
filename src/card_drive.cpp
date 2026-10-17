#include "card_drive.h"

#include "card_image.h"
#include "pc_card.h"
#include "res/resource.h"
#include "win/dialog.h"

namespace fs = std::filesystem;

CardDrive::CardDrive(EmulatorThread& emulator, Settings& settings, TextEncoding encoding)
    : Drive(emulator, settings, encoding) {
  texts_.caption = L"Karta";
  texts_.nominative = L"karta";
  texts_.accusative = L"kartu";
  texts_.locative = L"na kartě";
  texts_.onto = L"na kartu";
  texts_.without = L"bez karty";
  texts_.inside = L"ve slotu";
  texts_.menu = L"Karta";
  texts_.image_title = L"Vyberte obraz karty";
  texts_.image_filter = L"Obrazy karet";
  texts_.image_pattern = card_image::kPattern;
  texts_.damaged_dump = L"karta-poskozena.img";
  texts_.slots.caption = L"Sloty s kartami";
  texts_.slots.current = L"Sem &vloženou kartu";
  texts_.slots.lock = L"&Zamknout tuto kartu proti zápisu";
  texts_.slots.folder_title = L"Vyberte složku s kartou pro tento slot";
  texts_.slots.image_title = L"Vyberte obraz karty pro tento slot";
  texts_.slots.image_filter = L"Obrazy karet";
  texts_.slots.image_pattern = card_image::kPattern;
}

void CardDrive::Insert(AriaMachine& machine, std::vector<uint8_t> image, bool locked) {
  // A .flash image is a flash card (card_image.h); everything else SRAM.
  const bool flash = !folder_ && card_image::IsFlash(home_);
  const uint32_t size = static_cast<uint32_t>(image.size());
  PcCard card = flash ? PcCard::Flash(size, std::move(image)) : PcCard::Sram(size, std::move(image));
  card.write_protected = locked;
  // The machine keeps the slot empty long enough for the drivers to see
  // the change (AriaMachine::kCardSwapTicks).
  machine.InsertCard(std::move(card));
}

std::optional<std::vector<uint8_t>> CardDrive::Take(AriaMachine& machine) {
  std::optional<PcCard> card = machine.EjectCard();
  if (!card) return std::nullopt;
  return card->Contents();
}

bool CardDrive::Writes(AriaMachine& machine, uint64_t& writes, bool& modified) {
  const PcCard* card = machine.SlotCard();
  if (!card) return false;
  writes = card->Writes();
  modified = card->Modified();
  return true;
}

std::vector<uint8_t> CardDrive::Contents(AriaMachine& machine) {
  const PcCard* card = machine.SlotCard();
  return card ? card->Contents() : std::vector<uint8_t>{};
}

void CardDrive::ClearModified(AriaMachine& machine, uint64_t writes) {
  if (PcCard* card = machine.SlotCard(); card && card->Writes() == writes) card->ClearModified();
}

void CardDrive::SetProtected(AriaMachine& machine, bool locked) {
  if (PcCard* card = machine.SlotCard()) card->write_protected = locked;
}

bool CardDrive::ReadImage(const fs::path& file, std::vector<uint8_t>& image, bool& readOnly, std::wstring& error) {
  if (!card_image::Read(file, image, error)) return false;
  std::error_code ec;
  const fs::perms perms = fs::status(file, ec).permissions();
  readOnly = !ec && (perms & fs::perms::owner_write) == fs::perms::none;
  return true;
}

bool CardDrive::WriteImage(const fs::path& file, const std::vector<uint8_t>& image, std::wstring& error) {
  return card_image::Write(file, image, error);
}

std::wstring CardDrive::SizeText() const { return CardSizeText(size_); }

bool CardDrive::KnownLocked(const std::wstring& path) const {
  const Settings::Card* known = settings_.FindCard(path);
  return known && known->locked;
}

void CardDrive::RememberLock(const std::wstring& path, bool locked) { settings_.SetCardLocked(path, locked); }

void CardDrive::RememberMedium(const std::wstring& path, uint32_t size, bool locked) {
  settings_.RememberCard(path, size, locked);
}

std::wstring CardDrive::TitlePart() const {
  // The card: the folder or image it lives in, as Eureka names a diskette's.
  if (!present()) return Drive::TitlePart();
  return (!folder_ && card_image::IsFlash(home_) ? L" — fleš karta " : L" — karta ") + home_.filename().wstring() +
         L", " + SizeText() + (locked_ ? L", zamčená" : L"");
}

void CardDrive::InsertFromFolder() {
  const std::wstring folder = win::PickFolder(owner_, L"Vyberte složku, ze které bude karta");
  if (folder.empty()) return;
  InsertFolderAskingSize(folder);
}

void CardDrive::InsertFolderAskingSize(const std::wstring& folder) {
  std::vector<uint32_t> sizes;
  uint64_t bytes = 0;
  std::wstring error;
  if (!FolderCard::FittingSizes(folder, encoding_, sizes, bytes, error)) {
    Tell(error, MB_ICONERROR);
    return;
  }
  if (sizes.empty()) {
    Tell(L"Složka „" + folder + L"“ se nevejde ani na největší kartu, 64 MB, nebo má v kořeni víc než 511 "
         L"položek.",
         MB_ICONERROR);
    return;
  }
  // A folder that was a card before comes back with its size offered first
  // and its lock, as the same real card would.
  const Settings::Card* known = settings_.FindCard(folder);
  CardSizeDialog dialog(fs::path(folder).filename().wstring(), bytes, sizes, known ? known->size : 0);
  if (dialog.ShowModal(owner_, IDD_CARD_SIZE) != IDOK) return;
  if (!MountFolder(folder, dialog.chosen(), known && known->locked)) return;
  ToneInserted();
  if (locked_) ToneLocked();
}

void CardDrive::InsertFolderSlot(const std::wstring& folder, bool locked) {
  // A folder that was a card goes in at its size, at once; one that never
  // was, or was only locked in the slots dialog, is asked for one.
  const Settings::Card* known = settings_.FindCard(folder);
  if (!known || known->size == 0) {
    InsertFolderAskingSize(folder);
    return;
  }
  if (!MountFolder(folder, known->size, locked)) return;
  ToneInserted();
  if (locked_) ToneLocked();
}

bool CardDrive::MountRememberedFolder(const std::wstring& folder, bool locked) {
  // A folder needs its size from the settings.
  const Settings::Card* known = settings_.FindCard(folder);
  if (!known || known->size == 0) {
    Tell(L"Karta ze složky „" + folder + L"“ z minulého spuštění se nevložila: velikost karty se nedochovala.",
         MB_ICONWARNING);
    return false;
  }
  return MountFolder(folder, known->size, locked);
}

bool CardDrive::MountFolder(const std::wstring& folder, uint32_t size, bool locked) {
  if (!EjectIfIn(folder)) return false;
  auto card = std::make_unique<FolderCard>();
  std::vector<uint8_t> image;
  std::wstring error;
  if (!card->Load(folder, size, encoding_, image, error)) {
    Tell(error, MB_ICONERROR);
    return false;
  }
  return PutIn(std::move(card), folder, std::move(image), locked);
}

void CardDrive::InsertNew() {
  NewCardDialog dialog;
  if (dialog.ShowModal(owner_, IDD_CARD_NEW) != IDOK) return;
  const uint32_t size = dialog.size();
  if (dialog.kind() == NewCardDialog::Kind::kFolder) {
    const std::wstring folder =
        win::PickFolderToCreate(owner_, L"Pojmenujte složku nové karty", L"Karta", DialogFolder(/*image=*/false).c_str());
    if (folder.empty()) return;
    std::error_code ec;
    if (fs::exists(folder, ec) && (!fs::is_directory(folder, ec) || !fs::is_empty(folder, ec))) {
      Tell(L"„" + folder + L"“ už existuje a není to prázdná složka. Nová karta potřebuje novou nebo prázdnou "
           L"složku; existující složku vložíte jako kartu přes F11, Ctrl+I.",
           MB_ICONERROR);
      return;
    }
    fs::create_directories(folder, ec);
    if (ec) {
      Tell(L"Složku „" + folder + L"“ nelze založit.", MB_ICONERROR);
      return;
    }
    if (!MountFolder(folder, size, /*locked=*/false)) return;
  } else {
    const bool flash = dialog.kind() == NewCardDialog::Kind::kFlash;
    const std::wstring file =
        win::PickFileToCreate(owner_, flash ? L"Pojmenujte obraz nové fleš karty" : L"Pojmenujte obraz nové karty",
                              flash ? L"Obrazy fleš karet" : L"Obrazy karet",
                              flash ? card_image::kFlashExtension : card_image::kExtension,
                              flash ? L"Fleš karta" : L"Karta", DialogFolder(/*image=*/true).c_str());
    if (file.empty()) return;
    // Writing over the image in the slot would lose it twice over: the
    // eject before the new card goes in would save the old card on top.
    if (present() && Settings::SamePath(home_.wstring(), file)) {
      Tell(L"Tento obraz je právě ve slotu. Novou kartu pojmenujte jinak, nebo starou nejdřív vysuňte.",
           MB_ICONERROR);
      return;
    }
    // Unformatted is erased: FFh, as a flash card leaves the factory.
    const std::vector<uint8_t> image =
        dialog.kind() == NewCardDialog::Kind::kImage
            ? FolderCard::Formatted(size, fs::path(file).stem().wstring(), encoding_)
            : std::vector<uint8_t>(size, 0xFF);
    std::wstring error;
    if (!card_image::Write(file, image, error)) {
      Tell(error, MB_ICONERROR);
      return;
    }
    // A file written over keeps nothing of the card it was, its lock
    // included.
    if (!MountImage(file, /*locked=*/false)) return;
  }
  ToneInserted();
}
