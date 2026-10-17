#ifndef ARIA_CARD_DIALOGS_H
#define ARIA_CARD_DIALOGS_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "settings.h"
#include "win/dialog.h"

// "512 KB", "1 MB": a card size as the user hears it.
std::wstring CardSizeText(uint32_t bytes);

// The card's size for a folder going into the slot (HANDOFF 6.8, ar-aj1):
// a list of the real sizes it fits on, as a real card is bought in one
// size. The default is `preferred` -- the size this folder had as a card
// before -- when it is still on the list, else the smallest of them that is
// at least 1 MB, so a nearly empty folder still leaves the Aria room to
// write.
class CardSizeDialog : public win::Dialog {
 public:
  CardSizeDialog(std::wstring folder, uint64_t bytes, std::vector<uint32_t> sizes, uint32_t preferred = 0)
      : folder_(std::move(folder)), bytes_(bytes), sizes_(std::move(sizes)), preferred_(preferred) {}

  uint32_t chosen() const { return chosen_; }

 protected:
  bool OnInit() override;
  bool OnCommand(int id, int notification) override;
  bool OnOk() override;

 private:
  std::wstring folder_;
  uint64_t bytes_;
  std::vector<uint32_t> sizes_;
  uint32_t preferred_;
  uint32_t chosen_ = 0;
};

// What a slot is called in the menu and the slots dialog: the name of its
// folder or image, or "(prázdný)".
std::wstring SlotDisplayName(const std::wstring& slot);

// How the slots dialog names the medium (cards, diskettes).
struct SlotTexts {
  std::wstring caption;       // "Sloty s kartami"
  std::wstring current;       // Button: "Sem &vloženou kartu"
  std::wstring lock;          // Check box: "&Zamknout tuto kartu proti zápisu"
  std::wstring folder_title;  // Folder picker; empty when the medium has no folders yet.
  std::wstring image_title;   // "Vyberte obraz karty pro tento slot"
  std::wstring image_filter;  // "Obrazy karet"
  std::wstring image_pattern;
};

// The nine slots (HANDOFF 6.8, ar-aj1), after the Eureka emulator's
// SlotsDialog: works on copies and hands them back on OK. The lock belongs
// to the medium, not to the slot, so two slots with one card share it, and
// a slot pointed at a card already locked arrives ticked.
class SlotsDialog : public win::Dialog {
 public:
  using Slots = std::array<std::wstring, Settings::kSlots>;
  using Locks = std::array<bool, Settings::kSlots>;

  // current is the home of the medium in the machine (empty for none) and
  // currentLocked its switch, so "Sem vloženou kartu" brings the truth
  // rather than what the settings last wrote down; knownLocked is the lock
  // the settings remember for any other.
  SlotsDialog(Slots slots, Locks locks, std::wstring current, bool currentLocked,
              std::function<bool(const std::wstring&)> knownLocked, std::wstring imageFolder, SlotTexts texts)
      : slots_(std::move(slots)),
        locks_(locks),
        currentCard_(std::move(current)),
        currentLocked_(currentLocked),
        knownLocked_(std::move(knownLocked)),
        imageFolder_(std::move(imageFolder)),
        texts_(std::move(texts)) {}

  const Slots& slots() const { return slots_; }
  const Locks& locks() const { return locks_; }

 protected:
  bool OnInit() override;
  bool OnCommand(int id, int notification) override;

 private:
  // Rebuilds the list with the selection kept, so a screen reader stays on
  // the line the user was working on.
  void FillList(int select);
  int Selected() const;
  void SetSlot(int index, std::wstring path, bool locked);
  void RefreshLock(int index);

  Slots slots_;
  Locks locks_{};
  std::wstring currentCard_;
  bool currentLocked_ = false;
  std::function<bool(const std::wstring&)> knownLocked_;
  std::wstring imageFolder_;
  SlotTexts texts_;
};

// A new card (HANDOFF 6.8, ar-aj1), after the Eureka emulator's Nová
// disketa: what kind and what size, every real size offered and 1 MB first.
// Where it goes, the window asks next.
class NewCardDialog : public win::Dialog {
 public:
  enum class Kind { kFolder, kImage, kBlankImage, kFlash };

  Kind kind() const { return kind_; }
  uint32_t size() const { return size_; }

 protected:
  bool OnInit() override;
  bool OnCommand(int id, int notification) override;
  bool OnOk() override;

 private:
  // The sizes for the kind chosen: every SRAM size, or a flash card's 2,
  // 4, 10 and 20 MB (Intel Series 2, HANDOFF 6.8). The selection keeps its
  // size when the new list has it.
  void FillSizes(bool flash);
  std::vector<uint32_t> sizes_;
  bool flashSizes_ = false;
  Kind kind_ = Kind::kFolder;
  uint32_t size_ = 0;
};

// A new diskette for the Backpack drive (HANDOFF 6.7, ar-l5v): formatted
// as 1.44 MB or 720 KB, or blank for the Aria's FORMAT. The file it goes
// in, the window asks next.
class NewDiskDialog : public win::Dialog {
 public:
  enum class Kind { kHigh, kLow, kBlank };
  Kind kind() const { return kind_; }

 protected:
  bool OnInit() override;
  bool OnOk() override;

 private:
  Kind kind_ = Kind::kHigh;
};

#endif
