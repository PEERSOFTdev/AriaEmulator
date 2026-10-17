#include "drive.h"

#include <fstream>
#include <iterator>

#include "res/resource.h"
#include "snapshot.h"
#include "win/dialog.h"

namespace fs = std::filesystem;

void Drive::ToneInserted() { Beep(660, 45); Beep(990, 45); }
void Drive::ToneEjected() { Beep(330, 90); }
void Drive::ToneLocked() { Beep(400, 140); }
void Drive::ToneUnlocked() { Beep(1000, 140); }

void Drive::Tell(const std::wstring& text, UINT icon) const {
  // At exit the window is gone already.
  MessageBoxW(IsWindow(owner_) ? owner_ : nullptr, text.c_str(), texts().caption.c_str(), MB_OK | icon);
}

void Drive::Changed() const {
  if (onChange_) onChange_();
}

void Drive::InsertFromImage() {
  const std::wstring file = win::PickFile(owner_, texts().image_title.c_str(), texts().image_filter.c_str(),
                                          texts().image_pattern.c_str(), DialogFolder(/*image=*/true).c_str());
  if (file.empty()) return;
  if (!MountImage(file, KnownLocked(file))) return;
  ToneInserted();
  if (locked_) ToneLocked();
}

void Drive::InsertSlot(int number) {
  const std::wstring path = Slot(number);
  // An empty slot answers rather than doing nothing: Ctrl+digit fires from
  // the accelerator table whatever the menu says, and silence is what a
  // hang sounds like (the Eureka's InsertSlot).
  if (path.empty()) {
    MessageBoxW(owner_,
                (L"Slot " + std::to_wstring(number) + L" je prázdný.\r\n\r\nPřiřadit mu " + texts().accusative +
                 L" můžete v nabídce " + texts().menu + L" → Spravovat sloty.")
                    .c_str(),
                texts().slots.caption.c_str(), MB_OK | MB_ICONINFORMATION);
    return;
  }
  std::error_code ec;
  const bool locked = KnownLocked(path);
  if (fs::is_directory(path, ec)) {
    InsertFolderSlot(path, locked);
    return;
  }
  if (!fs::is_regular_file(path, ec)) {
    // Named, so the user knows which of the nine has gone stale.
    MessageBoxW(owner_,
                (texts().caption + L" ze slotu " + std::to_wstring(number) + L" se nedá najít:\r\n\r\n" + path +
                 L"\r\n\r\nJe-li to odpojený disk, připojte ho. Jinak slotu přiřaďte jinou " + texts().accusative +
                 L" v nabídce " + texts().menu + L" → Spravovat sloty.")
                    .c_str(),
                texts().slots.caption.c_str(), MB_OK | MB_ICONWARNING);
    return;
  }
  if (!MountImage(path, locked)) return;
  ToneInserted();
  if (locked_) ToneLocked();
}

void Drive::ManageSlots() {
  SlotsDialog::Slots before;
  SlotsDialog::Locks locks{};
  for (int number = 1; number <= Settings::kSlots; ++number) {
    const auto index = static_cast<std::size_t>(number - 1);
    before[index] = Slot(number);
    if (before[index].empty()) continue;
    // The medium in the machine has the lock the machine says; Ctrl+Z
    // changes both, but this way it cannot drift.
    locks[index] = present() && Settings::SamePath(before[index], home_.wstring()) ? locked_
                                                                                   : KnownLocked(before[index]);
  }
  SlotsDialog dialog(before, locks, home_.wstring(), locked_,
                     [this](const std::wstring& path) { return KnownLocked(path); }, DialogFolder(/*image=*/true),
                     texts().slots);
  if (dialog.ShowModal(owner_, IDD_CARD_SLOTS) != IDOK) return;
  // The lock belongs to the medium, so it goes where Ctrl+Z writes it.
  bool inMachineLocked = locked_;
  for (int number = 1; number <= Settings::kSlots; ++number) {
    const auto index = static_cast<std::size_t>(number - 1);
    const std::wstring& path = dialog.slots()[index];
    SetSlot(number, path);
    if (path.empty()) continue;
    RememberLock(path, dialog.locks()[index]);
    if (present() && Settings::SamePath(path, home_.wstring())) inMachineLocked = dialog.locks()[index];
  }
  // The medium in the machine follows a lock changed here, with its tone.
  // ToggleLock saves the settings too.
  if (present() && inMachineLocked != locked_) {
    ToggleLock();
    return;
  }
  std::wstring error;
  if (!settings_.Save(error)) MessageBoxW(owner_, error.c_str(), texts().slots.caption.c_str(), MB_OK | MB_ICONWARNING);
}

void Drive::RefreshSlotItems(HMENU menu, UINT firstId) const {
  for (int number = 1; number <= Settings::kSlots; ++number) {
    const UINT id = firstId + static_cast<UINT>(number - 1);
    wchar_t existing[512];
    if (!GetMenuStringW(menu, id, existing, static_cast<int>(std::size(existing)), MF_BYCOMMAND)) continue;
    const std::wstring current(existing);
    const std::size_t tab = current.find(L'\t');
    if (tab == std::wstring::npos) continue;
    // The number is the mnemonic as well as the shortcut.
    std::wstring wanted = L"&" + std::to_wstring(number) + L" " + SlotDisplayName(Slot(number)) + current.substr(tab);
    if (wanted == current) continue;
    MENUITEMINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_STRING;
    info.dwTypeData = wanted.data();
    SetMenuItemInfoW(menu, id, FALSE, &info);
  }
}

bool Drive::EjectIfIn(const std::wstring& path) {
  // Put in again, the medium must come out first and be saved, or what the
  // Aria wrote since the last save would be read from the stale copy and
  // then overwritten.
  if (!present() || !Settings::SamePath(home_.wstring(), path)) return true;
  return Eject(/*quiet=*/true);
}

bool Drive::MountImage(const std::wstring& file, bool locked) {
  if (!EjectIfIn(file)) return false;
  std::vector<uint8_t> image;
  bool readOnly = false;
  std::wstring error;
  if (!ReadImage(file, image, readOnly, error)) {
    Tell(error, MB_ICONERROR);
    return false;
  }
  // Saved with the medium by Remember.
  SetImageFolder(fs::path(file).parent_path().wstring());
  return PutIn(nullptr, file, std::move(image), locked || readOnly, readOnly);
}

std::wstring Drive::DialogFolder(bool image) const {
  // Images open where the last image was; before there was one, and for a
  // new folder, beside the medium in the drive or the last one -- where the
  // user keeps them.
  std::error_code ec;
  if (image && fs::is_directory(ImageFolder(), ec)) return ImageFolder();
  const fs::path last = present() ? home_ : fs::path(LastPath());
  if (last.empty()) return {};
  return last.parent_path().wstring();
}

bool Drive::PutIn(std::unique_ptr<FolderCard> folder, const std::wstring& home, std::vector<uint8_t> image,
                  bool locked, bool readOnly) {
  if (present() && !Eject(/*quiet=*/true)) return false;
  const uint32_t size = static_cast<uint32_t>(image.size());
  // Home first: Insert may tell the medium's kind by it (.flash).
  folder_ = std::move(folder);
  home_ = home;
  emulator_.Call([&](AriaMachine& machine) { Insert(machine, std::move(image), locked); });
  size_ = size;
  locked_ = locked;
  readOnly_ = readOnly;
  owed_ = false;
  writes_ = 0;
  quietLooks_ = 0;
  autosaveFailed_ = false;
  Remember();
  Changed();
  return true;
}

void Drive::Remember() {
  if (present()) {
    // A read-only file is locked by the file, not by the user's switch.
    RememberMedium(home_.wstring(), size_, readOnly_ ? KnownLocked(home_.wstring()) : locked_);
    SetLastPath(home_.wstring());
  } else {
    SetLastPath(L"");
  }
  std::wstring error;
  if (!settings_.Save(error)) Tell(error, MB_ICONWARNING);
}

bool Drive::Save(const std::vector<uint8_t>& image, std::wstring& error) {
  // Nothing is ever written over a read-only file; the Aria could not
  // write to the medium either.
  if (readOnly_) return true;
  // An image takes the medium as it is, byte for byte; only a folder has
  // files that can fail to come off it.
  if (!folder_) return WriteImage(home_, image, error);
  std::vector<std::wstring> damaged;
  if (!folder_->Save(image, error, &damaged)) return false;
  if (damaged.empty() || damaged == damagedTold_) {
    damagedTold_ = damaged;
    return true;
  }
  damagedTold_ = damaged;
  // The image as the Aria left it, for a look at what broke (HANDOFF 6.8).
  std::wstring kept;
  const fs::path dump = snapshot::ConfigDirectory() / texts().damaged_dump;
  std::error_code ec;
  fs::create_directories(dump.parent_path(), ec);
  std::ofstream out(dump, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
  if (out) kept = L"\r\n\r\nObraz je uložen pro rozbor v „" + dump.wstring() + L"“.";
  std::wstring names;
  for (const std::wstring& name : damaged) names += L"\r\n" + name;
  Tell(L"Tyto soubory jsou " + texts().locative + L" poškozené a do složky „" + home_.wstring() +
           L"“ se nezapsaly; jejich kopie ve složce zůstala, jak byla:" + names +
           L"\r\n\r\nOstatní soubory se zapsaly." + kept,
       MB_ICONWARNING);
  return true;
}

void Drive::InsertRemembered() {
  const std::wstring path = LastPath();
  if (path.empty()) return;
  std::error_code ec;
  const bool folder = fs::is_directory(path, ec);
  const bool image = !folder && fs::is_regular_file(path, ec);
  const bool locked = KnownLocked(path);
  bool mounted = false;
  if (!folder && !image) {
    Tell(L"Z minulého spuštění se nevložila " + texts().nominative + L" „" + path +
             L"“: taková složka ani obraz už neexistuje.",
         MB_ICONWARNING);
  } else {
    // The mount says why when it cannot.
    mounted = folder ? MountRememberedFolder(path, locked) : MountImage(path, locked);
  }
  if (!mounted) {
    SetLastPath(L"");
    std::wstring ignored;
    settings_.Save(ignored);
  }
}

void Drive::Autosave() {
  if (!present()) return;
  // Every half second: the write count; once it has stood still for two
  // looks after a change, the medium goes home -- not mid-write, when a
  // directory being rewritten would make a file look deleted (the Eureka
  // emulator's rule, a second of quiet).
  uint64_t writes = 0;
  bool modified = false;
  emulator_.Call([&](AriaMachine& machine) { Writes(machine, writes, modified); });
  if (writes != writes_) {
    writes_ = writes;
    quietLooks_ = 0;
    autosaveFailed_ = false;
    return;
  }
  // After a failed save, the next try waits for the next change: a home
  // that refused once will refuse again a second later.
  if (!(modified || owed_) || autosaveFailed_ || ++quietLooks_ < 2) return;
  std::vector<uint8_t> image;
  emulator_.Call([&](AriaMachine& machine) { image = Contents(machine); });
  std::wstring error;
  if (!image.empty() && !Save(image, error)) {
    // Said once; eject and exit try again and say it too.
    autosaveFailed_ = true;
    Tell(error + L"\r\n\r\nEmulátor to zkusí znovu po další změně a při vysunutí.", MB_ICONWARNING);
    return;
  }
  // Saved: owed nothing, unless the Aria wrote again meanwhile.
  emulator_.Call([&](AriaMachine& machine) { ClearModified(machine, writes); });
  owed_ = false;
  // A diskette formatted to the other density changed its size (HANDOFF 6.7).
  if (!image.empty() && image.size() != size_) {
    size_ = static_cast<uint32_t>(image.size());
    Changed();
  }
}

std::optional<std::vector<uint8_t>> Drive::TakeOut(bool& written) {
  std::optional<std::vector<uint8_t>> out;
  emulator_.Call([&](AriaMachine& machine) {
    uint64_t writes = 0;
    bool modified = false;
    Writes(machine, writes, modified);
    written = modified || owed_;
    out = Take(machine);
  });
  return out;
}

bool Drive::Eject(bool quiet) {
  if (!present()) {
    // The shortcut on an empty drive: not silence, which sounds like a hang.
    if (!quiet) MessageBeep(MB_ICONASTERISK);
    return true;
  }
  bool written = false;
  std::optional<std::vector<uint8_t>> out = TakeOut(written);
  std::wstring error;
  if (out && written && !Save(*out, error)) {
    // It holds the only copy of what the Aria wrote: back in it goes, and
    // the drive remembers it still owes the save.
    emulator_.Call([&](AriaMachine& machine) { Insert(machine, std::move(*out), locked_); });
    owed_ = true;
    Tell(error + L"\r\n\r\n" + texts().caption + L" zůstala " + texts().inside + L".", MB_ICONERROR);
    return false;
  }
  owed_ = false;
  folder_.reset();
  home_.clear();
  locked_ = false;
  readOnly_ = false;
  damagedTold_.clear();
  Remember();
  if (!quiet) ToneEjected();
  Changed();
  return true;
}

void Drive::ToggleLock() {
  if (!present()) {
    MessageBeep(MB_ICONASTERISK);
    return;
  }
  if (readOnly_) {
    // The file decides; the switch cannot open it.
    Tell(L"Obraz „" + home_.wstring() + L"“ je ve Windows jen pro čtení, proto je " + texts().nominative +
             L" zamčená natrvalo. Odemknout ji jde jen zrušením atributu jen pro čtení u souboru a novým "
             L"vložením.",
         MB_ICONINFORMATION);
    return;
  }
  const bool locked = !locked_;
  emulator_.Call([&](AriaMachine& machine) { SetProtected(machine, locked); });
  locked_ = locked;
  Remember();
  if (locked) ToneLocked(); else ToneUnlocked();
  Changed();
}

void Drive::SaveOnExit() {
  if (!present()) return;
  bool written = false;
  std::optional<std::vector<uint8_t>> out = TakeOut(written);
  std::wstring error;
  if (out && written && !Save(*out, error)) {
    Tell(error + L"\r\n\r\nCo Aria " + texts().onto + L" zapsala od jejího vložení, se do složky ani obrazu "
                 L"nedostalo.",
         MB_ICONERROR);
  }
}

std::wstring Drive::TitlePart() const {
  if (!present()) return L" — " + texts().without;
  return L" — " + texts().nominative + L" " + home_.filename().wstring() + L", " + SizeText() +
         (locked_ ? L", zamčená" : L"");
}
