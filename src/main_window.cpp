#include "main_window.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>

#include "aria_version.h"
#include "card_dialogs.h"
#include "card_image.h"
#include "pc_card.h"
#include "res/resource.h"
#include "snapshot.h"
#include "win/dialog.h"

namespace {

constexpr wchar_t kClassName[] = L"AriaEmulatorWindow";
constexpr UINT_PTR kAudioTimer = 1;

// A mode nobody can hear is the trap this machine keeps setting: a keyboard
// that goes nowhere and a keyboard that is broken are both silence.  So every
// change of where the keys go says so out loud, in the host's own voice --
// Beep() goes to the default device, not through the emulated DSP, so it
// cannot be mistaken for the Aria speaking.  The shapes are the Eureka
// emulator's, note for note, so that its users hear the same thing.
void Tone(unsigned hz, unsigned ms) { Beep(hz, ms); }

// Falling: the keys are leaving the Aria.  Rising: they are coming back.
void ToneLeaving() { Tone(880, 70); Tone(440, 90); }
void ToneReturning() { Tone(440, 70); Tone(880, 90); }
void ToneArmed() { Tone(1200, 50); }
void ToneDisarmed() { Tone(600, 50); }

// A card going in and coming out, and its write-protect switch: the Eureka
// emulator's diskette tones note for note, two short notes up for in, one
// low for out, one long note low for locked and high for unlocked.
void ToneInserted() { Tone(660, 45); Tone(990, 45); }
void ToneEjected() { Tone(330, 90); }
void ToneLocked() { Tone(400, 140); }
void ToneUnlocked() { Tone(1000, 140); }

// The power switch, the only three-note shape: what follows switching off
// is silence, and silence is also what a crashed emulator sounds like.
// Three notes down for off, three up for on.
void TonePoweredOff() { Tone(700, 100); Tone(500, 100); Tone(300, 150); }
void TonePoweredOn() { Tone(300, 100); Tone(500, 100); Tone(700, 150); }

// A host key as a key of the Aria's matrix (HANDOFF 6.4, from BKB.COM's
// decoding at +02E5h-+0489h): row 0 = port 0581h (bits 0-5 dots 3, 2, 1,
// 4, 5, 6, the keys' order from left to right; bit 7 the space bar), row
// 1 = 0582h (bits 0-7 F1-F8), row 2 = 0584h (bits 0-3 the cursor cross up,
// down, left, right; bit 6 the central Shift). The dot order shows in BKB's
// chord list (+052Eh-+0578h): space + 1Bh and 1Ch are API.DOC's time
// (dots 2345) and date (dots 145) only this way round. The host layout
// follows the Eureka emulator: F D S and J K L are dots 1-3 and 4-6. Home,
// End, PgUp and PgDn press two arrows at once, as the Aria API defines them
// (Home = up + left, PgDn = down + right).
struct AriaKey {
  int row = -1;
  uint8_t bits = 0;
};

AriaKey KeyFor(WPARAM key) {
  switch (key) {
    case 'F': return {0, 0x04};
    case 'D': return {0, 0x02};
    case 'S': return {0, 0x01};
    case 'J': return {0, 0x08};
    case 'K': return {0, 0x10};
    case 'L': return {0, 0x20};
    case VK_SPACE: return {0, 0x80};
    case VK_UP: return {2, 0x01};
    case VK_DOWN: return {2, 0x02};
    case VK_LEFT: return {2, 0x04};
    case VK_RIGHT: return {2, 0x08};
    case VK_HOME: return {2, 0x05};
    case VK_PRIOR: return {2, 0x09};
    case VK_END: return {2, 0x06};
    case VK_NEXT: return {2, 0x0A};
    case VK_SHIFT: return {2, 0x40};
    default:
      if (key >= VK_F1 && key <= VK_F8) return {1, static_cast<uint8_t>(1 << (key - VK_F1))};
      return {};
  }
}

const wchar_t kShortcutHelp[] =
    L"Klávesy Arie:\r\n"
    L"\r\n"
    L"F, D, S jsou body 1, 2, 3 a J, K, L body 4, 5, 6, mezerník je\r\n"
    L"mezerník. F1 až F8 jsou funkční klávesy Arie. Šipky jsou kurzorový\r\n"
    L"kříž a Shift je kulatá klávesa uprostřed něj; samotný ztiší řeč.\r\n"
    L"Home, End, PgUp a PgDn stisknou dvě šipky naráz.\r\n"
    L"\r\n"
    L"Okno:\r\n"
    L"\r\n"
    L"F12 otevře nabídku. Alt ani F10 ji neotevřou, dokud má klávesnici\r\n"
    L"Aria.\r\n"
    L"\r\n"
    L"F11 pustí do Windows jednu následující klávesu nebo zkratku. Vysoký\r\n"
    L"tón znamená, že je nachystaná, nižší, že se minula.\r\n"
    L"\r\n"
    L"Shift+F11 uvolní klávesnici úplně a okno se chová jako každé jiné.\r\n"
    L"Klesající dvojice tónů znamená, že klávesy Arii opouštějí, stoupající,\r\n"
    L"že se vracejí. Stav je i v titulku okna (NVDA+T).\r\n"
    L"\r\n"
    L"Zkratky s Ctrl platí po F11, nebo rovnou, když je klávesnice\r\n"
    L"uvolněná: Ctrl+Q ukončí emulátor, Ctrl+H zobrazí tuto nápovědu.\r\n"
    L"Alt+F4 zavře okno vždy, Aria klávesu Alt nemá.\r\n"
    L"\r\n"
    L"Stroj:\r\n"
    L"\r\n"
    L"Arii vypnete jako naozaj příkazem konec, všemi čtyřmi šipkami\r\n"
    L"naráz. Většina klávesnic PC čtyři šipky naráz nepozná, proto je\r\n"
    L"konec i v nabídce Klávesnice, Poslat Arii klávesu (F12, K, P, K),\r\n"
    L"a stačí i Home s PgDn. Nebo F11, Ctrl+V. Vypnutá zůstane\r\n"
    L"v okně, ohlásí to tři klesající tóny a titulek, a klávesnici\r\n"
    L"dostanou Windows.\r\n"
    L"\r\n"
    L"F11, Ctrl+P je teplý reset: vypnutou Arii zapne, běžící spustí znovu.\r\n"
    L"Paměť i RAM disk D: zůstanou, jako když je drží baterie. Tři\r\n"
    L"stoupající tóny.\r\n"
    L"\r\n"
    L"F11, Ctrl+R odpojí baterii: paměť i RAM disk se ztratí a Aria začne\r\n"
    L"od začátku. Emulátor se nejdřív zeptá.\r\n"
    L"\r\n"
    L"Když Arii vypnete a pak zavřete okno, paměť se uloží a příští\r\n"
    L"spuštění na ni naváže. Zavření běžící Arie se zeptá, co s pamětí.\r\n"
    L"\r\n"
    L"Karta:\r\n"
    L"\r\n"
    L"F11, Ctrl+I vloží do slotu kartu SRAM ze složky ve Windows; Aria ji\r\n"
    L"vidí jako jednotku C:. Emulátor se zeptá na velikost karty a nabídne\r\n"
    L"jen ty, na které se složka vejde. F11, Ctrl+0 kartu vysune a zapíše\r\n"
    L"zpět do složky; při ukončení emulátoru se zapíše sama. Co Aria\r\n"
    L"smazala, najdete ve složce .aria-trash. F11, Ctrl+Z kartu zamkne\r\n"
    L"proti zápisu a znovu odemkne. Vložení zní dvěma krátkými stoupajícími\r\n"
    L"tóny, vysunutí jedním nízkým; karta je i v titulku okna.\r\n"
    L"\r\n"
    L"F11, Ctrl+O vloží kartu z obrazu, souboru s obsahem karty bajt po\r\n"
    L"bajtu. F11, Ctrl+M založí novou kartu: prázdnou složku, nebo obraz\r\n"
    L"naformátovaný či nenaformátovaný, který naformátuje až Aria.";

}  // namespace

MainWindow::MainWindow(EmulatorThread& emulator, MachineInfo info, Settings& settings)
    : emulator_(emulator), info_(std::move(info)), settings_(settings) {}

bool MainWindow::Create() {
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  HMENU menu = LoadMenuW(instance, MAKEINTRESOURCEW(IDR_MAIN_MENU));
  accelerators_ = LoadAcceleratorsW(instance, MAKEINTRESOURCEW(IDR_ACCELERATORS));
  hostAccelerators_ = LoadAcceleratorsW(instance, MAKEINTRESOURCEW(IDR_ACCELERATORS_HOST));
  // Resizable on purpose: nothing to lay out, but a fixed window cannot be
  // maximised or snapped, and a screen reader user does that as much as
  // anyone.
  if (!win::Window::Create(kClassName, L"Aria", WS_OVERLAPPEDWINDOW, 520, 260, menu)) return false;
  RegisterCommands();
  emulator_.SetNotifyWindow(hwnd_);
  RefreshTitle();
  // A slow timer, so that a sound device that failed to open is reported
  // even when nothing happens to the window.
  SetTimer(hwnd_, kAudioTimer, 500, nullptr);
  return true;
}

void MainWindow::RegisterCommands() {
  OnCommand(ID_FILE_EXIT, [this] { PostMessageW(hwnd_, WM_CLOSE, 0, 0); });
  OnCommand(ID_MACHINE_RESET, [this] {
    // Unlike the Eureka emulator's Reset, this one asks: on the Aria the
    // battery holds the RAM disk D:, files the user wrote, not only a
    // session.
    const int answer = MessageBoxW(hwnd_,
                                   L"Reset odpojí baterii: obsah paměti i RAM disku D: se ztratí a Aria "
                                   L"začne od začátku, jako po výměně baterie.\r\n\r\n"
                                   L"Odpojit baterii?",
                                   L"Reset Arie", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (answer == IDYES) emulator_.PostBatteryCut();
  });
  OnCommand(ID_MACHINE_POWERON, [this] { emulator_.PostPowerOn(); });
  OnCommand(ID_MACHINE_POWEROFF, [this] { emulator_.PostPowerOff(); });
  OnCommand(ID_KEYBOARD_RELEASE, [this] { SetReleased(!released_); });
  // Pressing F11 again changes your mind rather than arming it twice.
  OnCommand(ID_KEYBOARD_PASSONCE, [this] { SetPassOnce(!passOnce_); });
  OnCommand(ID_KEYBOARD_SEND_KONEC, [this] {
    // "Konec": all four cursor keys at once, which most PC keyboards cannot
    // report (key rollover). Down and up together; the emulator thread holds
    // the chord until BKB has taken it (EmulatorThread::ApplyKeys).
    emulator_.Key(2, 0x0F, true);
    emulator_.Key(2, 0x0F, false);
  });
  OnCommand(ID_CARD_INSERT, [this] { InsertCardFromFolder(); });
  OnCommand(ID_CARD_INSERT_IMAGE, [this] { InsertCardFromImage(); });
  OnCommand(ID_CARD_NEW, [this] { InsertNewCard(); });
  OnCommand(ID_CARD_EJECT, [this] { EjectCard(/*quiet=*/false); });
  OnCommand(ID_CARD_PROTECT, [this] { ToggleCardLock(); });
  OnCommand(ID_ACTIVATE_MENU, [this] {
    // What Alt and F10 would do, on a key the Aria has no use for.  Posted
    // rather than sent, so the menu loop does not start inside the
    // accelerator dispatch that asked for it.
    PostMessageW(hwnd_, WM_SYSCOMMAND, SC_KEYMENU, 0);
  });
  OnCommand(ID_HELP_KEYS, [this] {
    MessageBoxW(hwnd_, kShortcutHelp, L"Klávesové zkratky", MB_OK | MB_ICONINFORMATION);
  });
  OnCommand(ID_HELP_ABOUT, [this] {
    MessageBoxW(hwnd_, AboutText().c_str(), L"O programu", MB_OK | MB_ICONINFORMATION);
  });
}

void MainWindow::InsertCardFromFolder() {
  const std::wstring folder = win::PickFolder(hwnd_, L"Vyberte složku, ze které bude karta");
  if (folder.empty()) return;
  std::vector<uint32_t> sizes;
  uint64_t bytes = 0;
  std::wstring error;
  if (!FolderCard::FittingSizes(folder, info_.text_encoding, sizes, bytes, error)) {
    MessageBoxW(hwnd_, error.c_str(), L"Karta", MB_OK | MB_ICONERROR);
    return;
  }
  if (sizes.empty()) {
    MessageBoxW(hwnd_,
                (L"Složka „" + folder + L"“ se nevejde ani na největší kartu, 64 MB, nebo má v kořeni víc "
                 L"než 511 položek.").c_str(),
                L"Karta", MB_OK | MB_ICONERROR);
    return;
  }
  // A folder that was a card before comes back with its size offered first
  // and its lock, as the same real card would.
  const Settings::Card* known = settings_.FindCard(folder);
  CardSizeDialog dialog(std::filesystem::path(folder).filename().wstring(), bytes, sizes, known ? known->size : 0);
  if (dialog.ShowModal(hwnd_, IDD_CARD_SIZE) != IDOK) return;
  if (!MountFolderCard(folder, dialog.chosen(), known && known->locked)) return;
  ToneInserted();
  if (cardLocked_) ToneLocked();
}

void MainWindow::InsertCardFromImage() {
  const std::wstring file = win::PickFile(hwnd_, L"Vyberte obraz karty", L"Obrazy karet", card_image::kExtension,
                                          CardDialogFolder(/*image=*/true).c_str());
  if (file.empty()) return;
  const Settings::Card* known = settings_.FindCard(file);
  if (!MountImageCard(file, known && known->locked)) return;
  ToneInserted();
  if (cardLocked_) ToneLocked();
}

void MainWindow::InsertNewCard() {
  NewCardDialog dialog;
  if (dialog.ShowModal(hwnd_, IDD_CARD_NEW) != IDOK) return;
  const uint32_t size = dialog.size();
  if (dialog.kind() == NewCardDialog::Kind::kFolder) {
    const std::wstring folder = win::PickFolderToCreate(hwnd_, L"Pojmenujte složku nové karty", L"Karta",
                                                        CardDialogFolder(/*image=*/false).c_str());
    if (folder.empty()) return;
    std::error_code ec;
    if (std::filesystem::exists(folder, ec) &&
        (!std::filesystem::is_directory(folder, ec) || !std::filesystem::is_empty(folder, ec))) {
      MessageBoxW(hwnd_, (L"„" + folder + L"“ už existuje a není to prázdná složka. Nová karta potřebuje novou "
                          L"nebo prázdnou složku; existující složku vložíte jako kartu přes F11, Ctrl+I.")
                             .c_str(),
                  L"Karta", MB_OK | MB_ICONERROR);
      return;
    }
    std::filesystem::create_directories(folder, ec);
    if (ec) {
      MessageBoxW(hwnd_, (L"Složku „" + folder + L"“ nelze založit.").c_str(), L"Karta", MB_OK | MB_ICONERROR);
      return;
    }
    if (!MountFolderCard(folder, size, /*locked=*/false)) return;
  } else {
    const std::wstring file = win::PickFileToCreate(hwnd_, L"Pojmenujte obraz nové karty", L"Obrazy karet",
                                                    card_image::kExtension, L"Karta",
                                                    CardDialogFolder(/*image=*/true).c_str());
    if (file.empty()) return;
    // Writing over the image in the slot would lose it twice over: the
    // eject before the new card goes in would save the old card on top.
    if (!cardHome_.empty() && Settings::SamePath(cardHome_.wstring(), file)) {
      MessageBoxW(hwnd_, L"Tento obraz je právě ve slotu. Novou kartu pojmenujte jinak, nebo starou nejdřív "
                         L"vysuňte.",
                  L"Karta", MB_OK | MB_ICONERROR);
      return;
    }
    // Unformatted is erased: FFh, as a flash card leaves the factory.
    const std::vector<uint8_t> image =
        dialog.kind() == NewCardDialog::Kind::kImage
            ? FolderCard::Formatted(size, std::filesystem::path(file).stem().wstring(), info_.text_encoding)
            : std::vector<uint8_t>(size, 0xFF);
    std::wstring error;
    if (!card_image::Write(file, image, error)) {
      MessageBoxW(hwnd_, error.c_str(), L"Karta", MB_OK | MB_ICONERROR);
      return;
    }
    // A file written over keeps nothing of the card it was, its lock
    // included.
    if (!MountImageCard(file, /*locked=*/false)) return;
  }
  ToneInserted();
}

bool MainWindow::EjectIfInSlot(const std::wstring& path) {
  // Put in again, the card must come out first and be saved, or what the
  // Aria wrote since the last save would be read from the stale copy and
  // then overwritten.
  if (cardHome_.empty() || !Settings::SamePath(cardHome_.wstring(), path)) return true;
  return EjectCard(/*quiet=*/true);
}

bool MainWindow::MountFolderCard(const std::wstring& folder, uint32_t size, bool locked) {
  if (!EjectIfInSlot(folder)) return false;
  auto card = std::make_unique<FolderCard>();
  std::vector<uint8_t> image;
  std::wstring error;
  if (!card->Load(folder, size, info_.text_encoding, image, error)) {
    MessageBoxW(hwnd_, error.c_str(), L"Karta", MB_OK | MB_ICONERROR);
    return false;
  }
  return PutCardIn(std::move(card), folder, std::move(image), locked);
}

bool MainWindow::MountImageCard(const std::wstring& file, bool locked) {
  if (!EjectIfInSlot(file)) return false;
  std::vector<uint8_t> image;
  std::wstring error;
  if (!card_image::Read(file, image, error)) {
    MessageBoxW(hwnd_, error.c_str(), L"Karta", MB_OK | MB_ICONERROR);
    return false;
  }
  // Saved with the card by RememberCard.
  settings_.SetImageFolder(std::filesystem::path(file).parent_path().wstring());
  return PutCardIn(nullptr, file, std::move(image), locked);
}

std::wstring MainWindow::CardDialogFolder(bool image) const {
  // Images open where the last image was; before there was one, and for a
  // new folder, beside the card in the slot or the last one -- where the
  // user keeps cards.
  std::error_code ec;
  if (image && std::filesystem::is_directory(settings_.image_folder(), ec)) return settings_.image_folder();
  const std::filesystem::path card = !cardHome_.empty() ? cardHome_ : std::filesystem::path(settings_.last_card());
  if (card.empty()) return {};
  return card.parent_path().wstring();
}

bool MainWindow::PutCardIn(std::unique_ptr<FolderCard> folder, const std::wstring& home, std::vector<uint8_t> image,
                           bool locked) {
  // The card in the slot goes back to its home first; the machine keeps
  // the slot empty long enough for the drivers to see the change
  // (AriaMachine::kCardSwapTicks).
  if (!cardHome_.empty() && !EjectCard(/*quiet=*/true)) return false;
  const uint32_t size = static_cast<uint32_t>(image.size());
  PcCard inserted = PcCard::Sram(size, std::move(image));
  inserted.write_protected = locked;
  emulator_.Call([&](AriaMachine& machine) { machine.InsertCard(std::move(inserted)); });
  cardFolder_ = std::move(folder);
  cardHome_ = home;
  cardSize_ = size;
  cardLocked_ = locked;
  cardWrites_ = 0;
  cardQuietLooks_ = 0;
  autosaveFailed_ = false;
  RememberCard();
  RefreshTitle();
  return true;
}

void MainWindow::RememberCard() {
  if (!cardHome_.empty()) {
    settings_.RememberCard(cardHome_.wstring(), cardSize_, cardLocked_);
    settings_.SetLastCard(cardHome_.wstring());
  } else {
    settings_.SetLastCard(L"");
  }
  std::wstring error;
  if (!settings_.Save(error)) MessageBoxW(hwnd_, error.c_str(), L"Karta", MB_OK | MB_ICONWARNING);
}

bool MainWindow::SaveCard(const std::vector<uint8_t>& image, std::wstring& error) {
  // An image takes the card as it is, byte for byte; only a folder has
  // files that can fail to come off it.
  if (!cardFolder_) return card_image::Write(cardHome_, image, error);
  std::vector<std::wstring> damaged;
  if (!cardFolder_->Save(image, error, &damaged)) return false;
  if (damaged.empty() || damaged == damagedTold_) {
    damagedTold_ = damaged;
    return true;
  }
  damagedTold_ = damaged;
  // The image as the Aria left it, for a look at what broke (HANDOFF 6.8).
  std::wstring kept;
  const std::filesystem::path dump = snapshot::ConfigDirectory() / L"karta-poskozena.img";
  std::error_code ec;
  std::filesystem::create_directories(dump.parent_path(), ec);
  std::ofstream out(dump, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
  if (out) kept = L"\r\n\r\nObraz karty je uložen pro rozbor v „" + dump.wstring() + L"“.";
  std::wstring names;
  for (const std::wstring& name : damaged) names += L"\r\n" + name;
  // At exit the window is gone already.
  MessageBoxW(IsWindow(hwnd_) ? hwnd_ : nullptr,
              (L"Tyto soubory jsou na kartě poškozené a do složky „" + cardHome_.wstring() +
               L"“ se nezapsaly; jejich kopie ve složce zůstala, jak byla:" + names +
               L"\r\n\r\nOstatní soubory se zapsaly." + kept)
                  .c_str(),
              L"Karta", MB_OK | MB_ICONWARNING);
  return true;
}

void MainWindow::InsertRememberedCard() {
  const std::wstring path = settings_.last_card();
  if (path.empty()) return;
  const Settings::Card* known = settings_.FindCard(path);
  std::error_code ec;
  // A folder needs its size from the settings; an image has its own.
  const bool folder = known && std::filesystem::is_directory(path, ec);
  const bool image = !folder && std::filesystem::is_regular_file(path, ec);
  if (!folder && !image) {
    MessageBoxW(hwnd_, (L"Karta „" + path + L"“ z minulého spuštění se nevložila: taková složka ani obraz "
                        L"karty už neexistuje.").c_str(),
                L"Karta", MB_OK | MB_ICONWARNING);
    settings_.SetLastCard(L"");
    std::wstring ignored;
    settings_.Save(ignored);
    return;
  }
  const bool locked = known && known->locked;
  // The mount says why when the folder has outgrown its card or the image
  // cannot be read.
  if (folder ? !MountFolderCard(path, known->size, locked) : !MountImageCard(path, locked)) {
    settings_.SetLastCard(L"");
    std::wstring ignored;
    settings_.Save(ignored);
  }
}

void MainWindow::AutosaveCard() {
  if (cardHome_.empty()) return;
  // Every half second: the card's write count; once it has stood still for
  // two looks after a change, the card goes to its folder -- not mid-write,
  // when a directory being rewritten would make a file look deleted (the
  // Eureka emulator's rule, a second of quiet).
  uint64_t writes = 0;
  bool modified = false;
  emulator_.Call([&](AriaMachine& machine) {
    if (const PcCard* card = machine.SlotCard()) {
      writes = card->Writes();
      modified = card->Modified();
    }
  });
  if (writes != cardWrites_) {
    cardWrites_ = writes;
    cardQuietLooks_ = 0;
    autosaveFailed_ = false;
    return;
  }
  // After a failed save, the next try waits for the next change: a folder
  // that refused once will refuse again a second later.
  if (!modified || autosaveFailed_ || ++cardQuietLooks_ < 2) return;
  std::vector<uint8_t> image;
  emulator_.Call([&](AriaMachine& machine) {
    if (const PcCard* card = machine.SlotCard()) image = card->Contents();
  });
  std::wstring error;
  if (!image.empty() && !SaveCard(image, error)) {
    // Said once; eject and exit try again and say it too.
    autosaveFailed_ = true;
    MessageBoxW(hwnd_, (error + L"\r\n\r\nEmulátor to zkusí znovu po další změně a při vysunutí.").c_str(),
                L"Karta", MB_OK | MB_ICONWARNING);
    return;
  }
  // Saved: owed nothing, unless the Aria wrote again meanwhile.
  emulator_.Call([&](AriaMachine& machine) {
    if (PcCard* card = machine.SlotCard(); card && card->Writes() == writes) card->ClearModified();
  });
}

bool MainWindow::EjectCard(bool quiet) {
  if (cardHome_.empty()) {
    // The shortcut on an empty slot: not silence, which sounds like a hang.
    if (!quiet) MessageBeep(MB_ICONASTERISK);
    return true;
  }
  std::optional<PcCard> out;
  emulator_.Call([&](AriaMachine& machine) { out = machine.EjectCard(); });
  std::wstring error;
  if (out && !SaveCard(out->Contents(), error)) {
    // The card holds the only copy of what the Aria wrote: back in it goes.
    emulator_.Call([&](AriaMachine& machine) { machine.InsertCard(std::move(*out)); });
    MessageBoxW(hwnd_, (error + L"\r\n\r\nKarta zůstala ve slotu.").c_str(), L"Karta", MB_OK | MB_ICONERROR);
    return false;
  }
  cardFolder_.reset();
  cardHome_.clear();
  cardLocked_ = false;
  damagedTold_.clear();
  RememberCard();
  if (!quiet) ToneEjected();
  RefreshTitle();
  return true;
}

void MainWindow::ToggleCardLock() {
  if (cardHome_.empty()) {
    MessageBeep(MB_ICONASTERISK);
    return;
  }
  const bool locked = !cardLocked_;
  emulator_.Call([&](AriaMachine& machine) {
    if (PcCard* card = machine.SlotCard()) card->write_protected = locked;
  });
  cardLocked_ = locked;
  RememberCard();
  if (locked) ToneLocked(); else ToneUnlocked();
  RefreshTitle();
}

void MainWindow::SaveCardOnExit() {
  if (cardHome_.empty()) return;
  std::optional<PcCard> out;
  emulator_.Call([&](AriaMachine& machine) { out = machine.EjectCard(); });
  std::wstring error;
  if (out && !SaveCard(out->Contents(), error)) {
    MessageBoxW(nullptr, (error + L"\r\n\r\nCo Aria na kartu zapsala od jejího vložení, se do složky nedostalo.")
                             .c_str(),
                L"Karta", MB_OK | MB_ICONERROR);
  }
}

std::wstring MainWindow::AboutText() const {
  return L"Aria Emulator\r\n"
         L"Verze " + std::wstring(ARIA_VERSION) + L"\r\n"
         L"\r\n"
         L"Emulátor hlasového zápisníku Robotron Aria pro nevidomé.\r\n"
         L"Procesor 80C188 na 15 MHz, signálový procesor ADSP-2105.\r\n"
         L"\r\n"
         L"Disk B: " + info_.flash + L"\r\n" +
         L"Sestavení: " + info_.version + L", " + info_.encoding + L"\r\n" +
         L"Jádro FreeDOS: " + info_.kernel + L"\r\n";
}

void MainWindow::RefreshTitle() const {
  // The state lives in the title because that is the one thing a screen
  // reader can be asked for at any moment (NVDA+T) and announces on Alt+Tab.
  // The tones say when it changed; the title answers "what is it now".  It
  // names no keys: advice in a state line is repeated on every glance and
  // rots quietly (the Eureka emulator learned that); the keys are in
  // Nápověda.
  // Switched off outranks where the keys go: they go nowhere.
  std::wstring title = L"Aria " + info_.version + L", " + info_.encoding +
                       (poweredOff_ ? L" — vypnutá"
                        : released_ ? L" — klávesnice uvolněná"
                                    : L" — klávesy do Arie");
  // The card: the folder or image it lives in, as Eureka names a diskette's.
  if (!cardHome_.empty()) {
    title += L" — karta " + cardHome_.filename().wstring() + L", " + CardSizeText(cardSize_) +
             (cardLocked_ ? L", zamčená" : L"");
  } else {
    title += L" — bez karty";
  }
  if (noAudio_) title += L" — bez zvuku";
  SetTitle(title);
}

void MainWindow::RefreshMenu() const {
  const HMENU menu = GetMenu(hwnd_);
  if (!menu) return;
  CheckMenuItem(menu, ID_KEYBOARD_RELEASE, MF_BYCOMMAND | (released_ ? MF_CHECKED : MF_UNCHECKED));
  // While the machine is off the keyboard is released and cannot be taken
  // back: there is nothing on the other side.  Ticked and greyed together.
  EnableMenuItem(menu, ID_KEYBOARD_RELEASE, MF_BYCOMMAND | (poweredOff_ ? MF_GRAYED : MF_ENABLED));
  // Nothing to switch off on an off machine.  Both resets stay enabled: the
  // warm one switches an off machine on, and on a running one it is the way
  // out of a hang.
  EnableMenuItem(menu, ID_MACHINE_POWEROFF, MF_BYCOMMAND | (poweredOff_ ? MF_GRAYED : MF_ENABLED));
  // A key for an off machine would go nowhere, in silence.
  EnableMenuItem(menu, ID_KEYBOARD_SEND_KONEC, MF_BYCOMMAND | (poweredOff_ ? MF_GRAYED : MF_ENABLED));
  // Nothing to take out or lock in an empty slot.
  EnableMenuItem(menu, ID_CARD_EJECT, MF_BYCOMMAND | (!cardHome_.empty() ? MF_ENABLED : MF_GRAYED));
  EnableMenuItem(menu, ID_CARD_PROTECT, MF_BYCOMMAND | (!cardHome_.empty() ? MF_ENABLED : MF_GRAYED));
  CheckMenuItem(menu, ID_CARD_PROTECT, MF_BYCOMMAND | (cardLocked_ ? MF_CHECKED : MF_UNCHECKED));
  CheckMenuItem(menu, ID_KEYBOARD_PASSONCE, MF_BYCOMMAND | (passOnce_ ? MF_CHECKED : MF_UNCHECKED));
  // Meaningless while the keyboard is released: every key already goes to
  // Windows.  Greyed says so where a screen reader reads it.
  EnableMenuItem(menu, ID_KEYBOARD_PASSONCE, MF_BYCOMMAND | (released_ ? MF_GRAYED : MF_ENABLED));
  RefreshShortcutText(menu);
}

// The Ctrl shortcuts need F11 in front of them only while the Aria has the
// keyboard.  Once it is released they stand on their own, and a menu still
// saying "F11, Ctrl+Q" would tell the user to press a key that does nothing
// in that state.  The letters are read back out of the menu, so the .rc stays
// the only place that names them.
void MainWindow::RefreshShortcutText(HMENU menu) const {
  static constexpr wchar_t kPrefix[] = L"F11, ";
  for (UINT id : {ID_FILE_EXIT, ID_MACHINE_RESET, ID_MACHINE_POWERON, ID_MACHINE_POWEROFF, ID_CARD_INSERT,
                  ID_CARD_INSERT_IMAGE, ID_CARD_NEW, ID_CARD_EJECT, ID_CARD_PROTECT, ID_HELP_KEYS}) {
    wchar_t text[128];
    // MF_BYCOMMAND searches the submenus too, so the ids are enough.
    if (!GetMenuStringW(menu, id, text, static_cast<int>(std::size(text)), MF_BYCOMMAND)) continue;
    std::wstring item(text);
    const size_t tab = item.find(L'\t');
    if (tab == std::wstring::npos) continue;
    std::wstring keys = item.substr(tab + 1);
    if (keys.starts_with(kPrefix)) keys.erase(0, std::size(kPrefix) - 1);
    if (!released_) keys.insert(0, kPrefix);
    std::wstring wanted = item.substr(0, tab) + L'\t' + keys;
    if (wanted == item) continue;
    // MIIM_STRING alone: ModifyMenu would take the item's other attributes
    // with it, and the checks and greying above are set by then.
    MENUITEMINFOW itemInfo{};
    itemInfo.cbSize = sizeof(itemInfo);
    itemInfo.fMask = MIIM_STRING;
    itemInfo.dwTypeData = wanted.data();
    SetMenuItemInfoW(menu, id, FALSE, &itemInfo);
  }
}

void MainWindow::ForwardKey(bool down, WPARAM virtualKey, LPARAM lParam) {
  const AriaKey aria = KeyFor(virtualKey);
  if (aria.row < 0) return;
  if (down) {
    // Windows' typematic repeat is not the Aria's; BKB repeats by itself.
    if (lParam & (1 << 30)) return;
    held_.push_back(virtualKey);
  } else {
    // Only a key whose press went to the Aria: one pressed while the host
    // had the keyboard was never down there.
    const auto found = std::find(held_.begin(), held_.end(), virtualKey);
    if (found == held_.end()) return;
    held_.erase(found);
  }
  emulator_.Key(aria.row, aria.bits, down);
}

void MainWindow::ReleaseAll() {
  for (WPARAM key : held_) {
    const AriaKey aria = KeyFor(key);
    emulator_.Key(aria.row, aria.bits, false);
  }
  held_.clear();
}

void MainWindow::SetReleased(bool released, bool quiet) {
  // An off machine has nothing to give the keyboard to.
  if (poweredOff_ && !released) return;
  if (released_ == released) return;
  released_ = released;
  // Arming one and then the other would leave the one-shot armed behind the
  // sticky mode, where nothing can spend it.
  passOnce_ = false;
  // Whatever the Aria believes is held has to go up now: no release for it
  // is ever coming while the keyboard is somewhere else.  Shift+F11 itself
  // pressed the Aria's Shift on the way.
  ReleaseAll();
  RefreshTitle();
  if (quiet) return;
  if (released_) ToneLeaving(); else ToneReturning();
}

bool MainWindow::ConfirmClose() {
  const int answer = MessageBoxW(hwnd_,
                                 L"Aria je stále zapnutá.\r\n\r\n"
                                 L"Ano: vypnout ji a paměť uložit, příští spuštění na ni naváže.\r\n"
                                 L"Ne: zavřít bez uložení, jako po odpojení baterie; příště bude paměť "
                                 L"i RAM disk D: prázdný.\r\n"
                                 L"Storno: nechat Arii běžet.\r\n\r\n"
                                 L"Vypnout Arii a uložit paměť?",
                                 L"Aria je zapnutá", MB_YESNOCANCEL | MB_ICONQUESTION);
  if (answer == IDCANCEL) return false;
  switchOffOnExit_ = answer == IDYES;
  return true;
}

void MainWindow::SetPassOnce(bool armed) {
  // Meaningless while the keyboard is released -- every key already goes to
  // Windows -- and saying so beats arming something that can never fire.
  if (released_) return;
  if (passOnce_ == armed) return;
  passOnce_ = armed;
  passOnceUsed_ = false;
  if (passOnce_) {
    ReleaseAll();
    ToneArmed();
  } else {
    ToneDisarmed();
  }
}

bool MainWindow::HostKeepsKey(bool down) {
  if (released_) return true;
  if (!passOnce_) return false;
  if (down) {
    passOnceUsed_ = true;
    return true;
  }
  // Spent on the first release whose press was actually seen here.  F11 arms
  // this from the accelerator table, which eats the F11 press but not its
  // release; without the guard F11 would disarm itself (measured in the
  // Eureka emulator: F11 then Alt did nothing).  Modifiers count, so Alt on
  // its own, which opens the menu on its release, spends it too.
  if (passOnceUsed_) {
    passOnce_ = false;
    passOnceUsed_ = false;
    ToneDisarmed();
  }
  return true;
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_KEYDOWN:
    case WM_KEYUP:
      // Breaking out hands the key to DefWindowProc, which is what "handled
      // by Windows normally" means for a window with no controls of its own.
      if (HostKeepsKey(message == WM_KEYDOWN)) break;
      ForwardKey(message == WM_KEYDOWN, wParam, lParam);
      return 0;

    // Alt and F10 arrive here.  While the Aria has the keyboard they are
    // swallowed, so that only F12 opens the menu: a stray Alt would otherwise
    // drop the user into the menu bar, and the next dot would pick an item.
    // Alt+F4 is the exception, unlike in the Eureka emulator, where Alt is a
    // key of the machine: the Aria has none, so Alt+F4 closes the window.
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      if (wParam == VK_F4 && message == WM_SYSKEYDOWN && (lParam & (1 << 29))) break;
      if (HostKeepsKey(message == WM_SYSKEYDOWN)) break;
      ForwardKey(message == WM_SYSKEYDOWN, wParam, lParam);
      return 0;

    // Swallowed so that a key does not end in the system beep -- but not
    // while the host owns the keyboard, because that is where the menu
    // mnemonics come from.
    case WM_SYSCHAR:
    case WM_CHAR:
      if (released_ || passOnce_) break;
      return 0;

    case WM_KILLFOCUS:
      // The releases of whatever is held will be delivered to whoever took
      // the focus, so let go now rather than let a key sit down for good --
      // with BKB's autorepeat that would be a dot typed without end.
      ReleaseAll();
      // An armed one-shot would otherwise be waiting on the way back, long
      // after the user forgot about it.  The sticky release is a deliberate
      // state and survives; this is not.
      SetPassOnce(false);
      return 0;

    case WM_ENTERMENULOOP:
      // F12 or a mouse click opens the menu without taking the focus, and the
      // menu loop eats the releases of keys still held.
      ReleaseAll();
      break;

    case WM_COMMAND:
      // Spends the one-shot on a shortcut that only fired because it was
      // armed: TranslateAccelerator takes the press, so HostKeepsKey never
      // sees it.  HIWORD 1 means "accelerator", so choosing the same command
      // from the menu does not spend anything.  The two keyboard-state
      // commands own that state and are left out.
      if (HIWORD(wParam) == 1 && LOWORD(wParam) != ID_KEYBOARD_PASSONCE && LOWORD(wParam) != ID_KEYBOARD_RELEASE)
        SetPassOnce(false);
      break;

    case WM_INITMENUPOPUP:
      RefreshMenu();
      break;

    case WM_CLOSE:
      // Every way out lands here: Konec posts it, Alt+F4 and the close box
      // reach it through WM_SYSCOMMAND.  An off machine has its memory
      // saved without a word; a running one asks (user's choice, 8. 10.
      // 2026; the Eureka emulator asks too).
      if (!poweredOff_ && !ConfirmClose()) return 0;
      break;

    case WM_ARIA_POWER:
      // Off and released are one state: an off machine has nowhere to put a
      // key, and holding the keyboard would make every key silent.  So the
      // keyboard goes to Windows on the way out and back on the way in,
      // without a tone of its own -- the power tone is the announcement.
      // poweredOff_ first: SetReleased asks it.
      poweredOff_ = wParam != 0;
      SetReleased(poweredOff_, /*quiet=*/true);
      if (poweredOff_) TonePoweredOff(); else TonePoweredOn();
      RefreshTitle();
      return 0;

    case WM_TIMER:
      if (wParam == kAudioTimer) AutosaveCard();
      if (wParam == kAudioTimer && !noAudio_ && !emulator_.AudioOk()) {
        // No sound device: say so in a way that is not the Aria's voice, and
        // keep it in the title.  The machine runs on regardless.
        noAudio_ = true;
        MessageBeep(MB_ICONHAND);
        RefreshTitle();
      }
      return 0;

    default:
      break;
  }
  return win::Window::HandleMessage(message, wParam, lParam);
}
