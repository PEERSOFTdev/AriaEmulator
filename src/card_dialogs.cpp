#include "card_dialogs.h"

#include <filesystem>

#include "card_folder.h"
#include "card_image.h"
#include "res/resource.h"

std::wstring CardSizeText(uint32_t bytes) {
  if (bytes >= 1024u * 1024u) return std::to_wstring(bytes / (1024u * 1024u)) + L" MB";
  return std::to_wstring(bytes / 1024u) + L" KB";
}

bool CardSizeDialog::OnInit() {
  const uint64_t kb = (bytes_ + 1023) / 1024;
  SetText(IDC_CARD_SIZE_LABEL, L"&Velikost karty pro složku „" + folder_ + L"“, soubory zabírají " +
                                   std::to_wstring(kb) + L" KB:");
  const HWND list = Item(IDC_CARD_SIZES);
  for (uint32_t size : sizes_) {
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(CardSizeText(size).c_str()));
  }
  // Sizes come smallest first: the first of 1 MB or more.
  std::size_t pick = 0;
  while (pick + 1 < sizes_.size() && sizes_[pick] < 1024u * 1024u) ++pick;
  for (std::size_t i = 0; i < sizes_.size(); ++i) {
    if (sizes_[i] == preferred_) pick = i;
  }
  SendMessageW(list, LB_SETCURSEL, pick, 0);
  SetFocus(list);
  return true;
}

bool CardSizeDialog::OnCommand(int id, int notification) {
  // A double click picks, as Enter does.
  if (id == IDC_CARD_SIZES && notification == LBN_DBLCLK) {
    if (OnOk()) EndDialog(hwnd_, IDOK);
    return true;
  }
  return false;
}

bool CardSizeDialog::OnOk() {
  const LRESULT index = SendMessageW(Item(IDC_CARD_SIZES), LB_GETCURSEL, 0, 0);
  if (index < 0 || static_cast<std::size_t>(index) >= sizes_.size()) return false;
  chosen_ = sizes_[static_cast<std::size_t>(index)];
  return true;
}

std::wstring SlotDisplayName(const std::wstring& slot) {
  if (slot.empty()) return L"(prázdný)";
  const std::filesystem::path path(slot);
  std::filesystem::path leaf = path.filename();
  // A trailing separator leaves filename() empty.
  if (leaf.empty()) leaf = path.parent_path().filename();
  return leaf.empty() ? slot : leaf.wstring();
}

namespace {

// One list line: the number first, because it is the shortcut, and the
// lock in the line itself, because the list is what is read while
// arrowing (the Eureka's SlotLine).
std::wstring SlotLine(int number, const std::wstring& slot, bool locked) {
  return std::to_wstring(number) + L": " + SlotDisplayName(slot) +
         (!slot.empty() && locked ? L", zamčená" : L"");
}

}  // namespace

bool SlotsDialog::OnInit() {
  FillList(0);
  return false;
}

void SlotsDialog::FillList(int select) {
  const HWND list = Item(IDC_SLOT_LIST);
  SendMessageW(list, LB_RESETCONTENT, 0, 0);
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    SendMessageW(list, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(SlotLine(static_cast<int>(i) + 1, slots_[i], locks_[i]).c_str()));
  }
  SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(select), 0);
  // Greyed rather than silent when the machine's slot is empty.
  SetEnabled(IDC_SLOT_CURRENT, !currentCard_.empty());
  RefreshLock(select);
}

void SlotsDialog::RefreshLock(int index) {
  if (index < 0 || index >= Settings::kSlots) return;
  const auto slot = static_cast<std::size_t>(index);
  SetChecked(IDC_SLOT_LOCK, locks_[slot]);
  // An empty slot has no card to lock.
  SetEnabled(IDC_SLOT_LOCK, !slots_[slot].empty());
}

int SlotsDialog::Selected() const {
  const LRESULT selected = SendMessageW(Item(IDC_SLOT_LIST), LB_GETCURSEL, 0, 0);
  return selected == LB_ERR ? -1 : static_cast<int>(selected);
}

void SlotsDialog::SetSlot(int index, std::wstring path, bool locked) {
  const auto slot = static_cast<std::size_t>(index);
  slots_[slot] = std::move(path);
  locks_[slot] = !slots_[slot].empty() && locked;
  FillList(index);
  SetFocus(Item(IDC_SLOT_LIST));
}

bool SlotsDialog::OnCommand(int id, int notification) {
  const int index = Selected();
  if (index < 0) return false;
  // A card pointed at arrives with the lock it has: the one in the machine
  // as the machine has it, any other as the settings remember it.
  auto lockOf = [&](const std::wstring& path) {
    if (!currentCard_.empty() && Settings::SamePath(path, currentCard_)) return currentLocked_;
    const Settings::Card* known = settings_.FindCard(path);
    return known && known->locked;
  };
  switch (id) {
    case IDC_SLOT_FOLDER: {
      const std::wstring folder = win::PickFolder(hwnd_, L"Vyberte složku s kartou pro tento slot");
      if (!folder.empty()) SetSlot(index, folder, lockOf(folder));
      return true;
    }
    case IDC_SLOT_IMAGE: {
      const std::wstring file = win::PickFile(hwnd_, L"Vyberte obraz karty pro tento slot", L"Obrazy karet",
                                              card_image::kPattern, imageFolder_.c_str());
      if (!file.empty()) SetSlot(index, file, lockOf(file));
      return true;
    }
    case IDC_SLOT_CURRENT:
      if (!currentCard_.empty()) SetSlot(index, currentCard_, currentLocked_);
      return true;
    case IDC_SLOT_CLEAR:
      SetSlot(index, L"", false);
      return true;
    case IDC_SLOT_LOCK: {
      const bool locked = IsChecked(IDC_SLOT_LOCK);
      const auto chosen = static_cast<std::size_t>(index);
      // One card, one lock: every slot holding it follows.
      for (std::size_t other = 0; other < slots_.size(); ++other) {
        if (other == chosen || (!slots_[other].empty() && Settings::SamePath(slots_[other], slots_[chosen])))
          locks_[other] = locked;
      }
      FillList(index);
      SetFocus(Item(IDC_SLOT_LOCK));
      return true;
    }
    case IDC_SLOT_LIST:
      if (notification == LBN_SELCHANGE) RefreshLock(index);
      return false;
    default:
      return false;
  }
}

void NewCardDialog::FillSizes(bool flash) {
  const HWND list = Item(IDC_CARD_NEW_SIZES);
  const LRESULT old = SendMessageW(list, LB_GETCURSEL, 0, 0);
  const uint32_t kept = old >= 0 && static_cast<std::size_t>(old) < sizes_.size() ? sizes_[old] : 0;
  sizes_.clear();
  if (flash) {
    for (uint32_t mb : {2u, 4u, 10u, 20u}) sizes_.push_back(mb * 1024u * 1024u);
  } else {
    for (uint32_t size = FolderCard::kMinSize; size <= FolderCard::kMaxSize; size *= 2) sizes_.push_back(size);
  }
  flashSizes_ = flash;
  SendMessageW(list, LB_RESETCONTENT, 0, 0);
  // The size chosen before when the new list has it, else 1 MB for SRAM
  // and 4 MB for flash.
  const uint32_t fallback = (flash ? 4u : 1u) * 1024u * 1024u;
  std::size_t pick = 0;
  for (std::size_t i = 0; i < sizes_.size(); ++i) {
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(CardSizeText(sizes_[i]).c_str()));
    if (sizes_[i] == fallback) pick = i;
  }
  for (std::size_t i = 0; i < sizes_.size(); ++i) {
    if (sizes_[i] == kept) pick = i;
  }
  SendMessageW(list, LB_SETCURSEL, pick, 0);
}

bool NewCardDialog::OnInit() {
  CheckRadioButton(hwnd_, IDC_CARD_NEW_FOLDER, IDC_CARD_NEW_FLASH, IDC_CARD_NEW_FOLDER);
  FillSizes(false);
  // The kind first, as it is read first.
  SetFocus(Item(IDC_CARD_NEW_FOLDER));
  return true;
}

bool NewCardDialog::OnCommand(int id, int notification) {
  if (id == IDC_CARD_NEW_SIZES && notification == LBN_DBLCLK) {
    if (OnOk()) EndDialog(hwnd_, IDOK);
    return true;
  }
  if (id >= IDC_CARD_NEW_FOLDER && id <= IDC_CARD_NEW_FLASH && notification == BN_CLICKED) {
    const bool flash = IsDlgButtonChecked(hwnd_, IDC_CARD_NEW_FLASH) == BST_CHECKED;
    if (flash != flashSizes_) FillSizes(flash);
    return false;
  }
  return false;
}

bool NewCardDialog::OnOk() {
  const LRESULT index = SendMessageW(Item(IDC_CARD_NEW_SIZES), LB_GETCURSEL, 0, 0);
  if (index < 0 || static_cast<std::size_t>(index) >= sizes_.size()) return false;
  size_ = sizes_[static_cast<std::size_t>(index)];
  kind_ = IsDlgButtonChecked(hwnd_, IDC_CARD_NEW_IMAGE) == BST_CHECKED   ? Kind::kImage
          : IsDlgButtonChecked(hwnd_, IDC_CARD_NEW_BLANK) == BST_CHECKED ? Kind::kBlankImage
          : IsDlgButtonChecked(hwnd_, IDC_CARD_NEW_FLASH) == BST_CHECKED ? Kind::kFlash
                                                                          : Kind::kFolder;
  return true;
}
