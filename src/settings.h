#ifndef ARIA_SETTINGS_H
#define ARIA_SETTINGS_H

// What the emulator remembers between runs, after the Eureka emulator's
// Settings: plain key=value in UTF-8, parsed by hand, because the profile
// API writes ANSI and a path with diacritics would silently break. The file
// sits beside the memory snapshot (snapshot::ConfigDirectory: a `config`
// folder beside the EXE, else %APPDATA%\Aria). A missing or unreadable file
// means defaults; a failed save is returned to the caller, which says so at
// the moment of the act that did not stick.
//
// Besides drive B: of the last run, it remembers cards (HANDOFF 6.8,
// ar-aj1): the one in the slot at the end of the last run, and for every
// folder or image file that has been
// a card its size and write-protect switch -- both belong to the card, as
// on a real one, so they come back whenever that card goes in again. Which
// of the two a card is, the file does not say: a path that is a folder is a
// folder card, any other an image (whose size is then the file's own).

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class Settings {
 public:
  explicit Settings(std::filesystem::path file) : file_(std::move(file)) {}

  // nastaveni.txt in snapshot::ConfigDirectory; empty when there is nowhere
  // to put it, which Save then reports.
  static std::filesystem::path FindFile();

  // A line that makes no sense is skipped rather than fatal: the file is
  // meant to be editable by hand.
  void Load();
  bool Save(std::wstring& error) const;

  // Drive B: of the last run (a firmware update, main.cpp), taken again
  // while it still loads, so that of several builds beside the EXE the one
  // chosen once with --flash stays; empty before the first run.
  const std::wstring& flash() const { return flash_; }
  void SetFlash(std::wstring path) { flash_ = std::move(path); }

  // The card (folder or image) to put back in at the next start; empty for
  // none.
  const std::wstring& last_card() const { return lastCard_; }
  void SetLastCard(std::wstring path) { lastCard_ = std::move(path); }

  // The folder of the last card image put in or made, where the image
  // dialogs open; empty until there was one.
  const std::wstring& image_folder() const { return imageFolder_; }
  void SetImageFolder(std::wstring folder) { imageFolder_ = std::move(folder); }

  struct Card {
    std::wstring path;
    uint32_t size = 0;
    bool locked = false;
  };
  // The card made from `path` before, or null. Paths compare by SamePath.
  // A size of 0 is a card whose lock is known but whose size is not yet: a
  // folder put in a slot and locked there before it was ever in the slot
  // of the machine.
  const Card* FindCard(const std::wstring& path) const;
  void RememberCard(const std::wstring& path, uint32_t size, bool locked);
  // The lock alone, the size kept (0 for a card not yet known).
  void SetCardLocked(const std::wstring& path, bool locked);

  // The nine slots of Ctrl+1 to Ctrl+9, after the Eureka emulator's: each a
  // folder or an image path, empty for none. Numbered 1..kSlots.
  static constexpr int kSlots = 9;
  const std::wstring& slot(int number) const { return slots_[static_cast<std::size_t>(number - 1)]; }
  void SetSlot(int number, std::wstring path) { slots_[static_cast<std::size_t>(number - 1)] = std::move(path); }

  // The Backpack floppy drive (HANDOFF 6.7, ar-l5v): whether its cable is
  // in (off unless the user connects it: connected, it takes E:, where the
  // old flash card format lives otherwise), the diskette in it at the end
  // of the last run, the folder of the last diskette image, and for every
  // image that has been a diskette its write-protect tab.
  bool backpack_connected() const { return backpackConnected_; }
  void SetBackpackConnected(bool connected) { backpackConnected_ = connected; }
  // The external keyboard's adapter on the serial line (HANDOFF 6.6,
  // ar-w6w), off unless the user connects it.
  bool keyboard_connected() const { return keyboardConnected_; }
  void SetKeyboardConnected(bool connected) { keyboardConnected_ = connected; }
  // Which PC keys are the dots, the layout's name as the file writes it
  // ("domovska", "horni"); empty means the default.  The window knows what
  // the names mean.
  const std::wstring& layout() const { return layout_; }
  void SetLayout(std::wstring name) { layout_ = std::move(name); }
  const std::wstring& last_disk() const { return lastDisk_; }
  void SetLastDisk(std::wstring path) { lastDisk_ = std::move(path); }
  const std::wstring& disk_folder() const { return diskFolder_; }
  void SetDiskFolder(std::wstring folder) { diskFolder_ = std::move(folder); }
  bool DiskLocked(const std::wstring& path) const;
  void SetDiskLocked(const std::wstring& path, bool locked);
  // Nine slots of their own, F11, Ctrl+F1 to F9.
  const std::wstring& disk_slot(int number) const { return diskSlots_[static_cast<std::size_t>(number - 1)]; }
  void SetDiskSlot(int number, std::wstring path) { diskSlots_[static_cast<std::size_t>(number - 1)] = std::move(path); }

  // Case-insensitive, separators and a trailing one folded, so the same
  // folder typed two ways is one card.
  static bool SamePath(const std::wstring& left, const std::wstring& right);

 private:
  std::filesystem::path file_;
  std::wstring flash_;
  std::wstring lastCard_;
  std::wstring imageFolder_;
  std::array<std::wstring, kSlots> slots_;
  // In the order they were first used, so the file stays readable.
  std::vector<Card> cards_;
  bool backpackConnected_ = false;
  bool keyboardConnected_ = false;
  std::wstring layout_;
  std::wstring lastDisk_;
  std::wstring diskFolder_;
  // Diskettes whose tab is closed; one that is not here is writable.
  std::vector<std::wstring> lockedDisks_;
  std::array<std::wstring, kSlots> diskSlots_;
};

#endif
