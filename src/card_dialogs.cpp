#include "card_dialogs.h"

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
