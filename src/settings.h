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
// So far it remembers cards (HANDOFF 6.8, ar-aj1): the one in the slot at
// the end of the last run, and for every folder or image file that has been
// a card its size and write-protect switch -- both belong to the card, as
// on a real one, so they come back whenever that card goes in again. Which
// of the two a card is, the file does not say: a path that is a folder is a
// folder card, any other an image (whose size is then the file's own).

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
  const Card* FindCard(const std::wstring& path) const;
  void RememberCard(const std::wstring& path, uint32_t size, bool locked);

  // Case-insensitive, separators and a trailing one folded, so the same
  // folder typed two ways is one card.
  static bool SamePath(const std::wstring& left, const std::wstring& right);

 private:
  std::filesystem::path file_;
  std::wstring lastCard_;
  std::wstring imageFolder_;
  // In the order they were first used, so the file stays readable.
  std::vector<Card> cards_;
};

#endif
