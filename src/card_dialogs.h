#ifndef ARIA_CARD_DIALOGS_H
#define ARIA_CARD_DIALOGS_H

#include <cstdint>
#include <string>
#include <vector>

#include "win/dialog.h"

// "512 KB", "1 MB": a card size as the user hears it.
std::wstring CardSizeText(uint32_t bytes);

// The card's size for a folder going into the slot (HANDOFF 6.8, ar-aj1):
// a list of the real sizes it fits on, as a real card is bought in one
// size. The default is the smallest of them that is at least 1 MB, so a
// nearly empty folder still leaves the Aria room to write.
class CardSizeDialog : public win::Dialog {
 public:
  CardSizeDialog(std::wstring folder, uint64_t bytes, std::vector<uint32_t> sizes)
      : folder_(std::move(folder)), bytes_(bytes), sizes_(std::move(sizes)) {}

  uint32_t chosen() const { return chosen_; }

 protected:
  bool OnInit() override;
  bool OnCommand(int id, int notification) override;
  bool OnOk() override;

 private:
  std::wstring folder_;
  uint64_t bytes_;
  std::vector<uint32_t> sizes_;
  uint32_t chosen_ = 0;
};

#endif
