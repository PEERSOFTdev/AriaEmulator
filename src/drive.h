#ifndef ARIA_DRIVE_H
#define ARIA_DRIVE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "card_dialogs.h"
#include "card_folder.h"
#include "emulator_thread.h"
#include "settings.h"
#include "text_codec.h"

// A drive for removable media as the window keeps it: the PC card slot
// (HANDOFF 6.8, ar-aj1) and the Backpack floppy drive (HANDOFF 6.7, ar-l5v),
// after the Eureka emulator's diskette. What both do alike lives here: the
// medium's home (an image file or a folder), its write-protect switch, the
// autosave after a second of quiet, saving on eject and on exit, putting
// the last one back at start, the nine slots, the tones and the title.
// What differs -- how the machine takes the medium, how an image is read
// and written, what the settings call it and how the messages name it --
// the two subclasses say.
class Drive {
 public:
  // How the messages and dialogs name the medium, in Czech.
  struct Texts {
    std::wstring caption;       // Message box title: "Karta".
    std::wstring nominative;    // "karta"
    std::wstring accusative;    // "kartu"
    std::wstring locative;      // "na kartě"
    std::wstring onto;          // "na kartu"
    std::wstring without;       // Title when empty: "bez karty".
    std::wstring inside;        // Where it sits: "ve slotu".
    std::wstring menu;          // The menu it is in: "Karta".
    std::wstring image_title;   // Open dialog: "Vyberte obraz karty".
    std::wstring image_filter;  // "Obrazy karet"
    std::wstring image_pattern;
    std::wstring damaged_dump;  // File name for an image with damaged files.
    SlotTexts slots;
  };

  Drive(EmulatorThread& emulator, Settings& settings, TextEncoding encoding)
      : emulator_(emulator), settings_(settings), encoding_(encoding) {}
  virtual ~Drive() = default;

  // The window that owns the dialogs, and what it does after every change
  // of what is in the drive or its lock (the title).
  void SetOwner(HWND owner, std::function<void()> onChange) {
    owner_ = owner;
    onChange_ = std::move(onChange);
  }

  bool present() const { return !home_.empty(); }
  bool locked() const { return locked_; }

  void InsertFromImage();
  // F11, Ctrl+1 to Ctrl+9: the medium a slot names. An empty or stale slot
  // is told, never silent.
  void InsertSlot(int number);
  void ManageSlots();
  // The slot items, from `firstId`, take the names of their media.
  void RefreshSlotItems(HMENU menu, UINT firstId) const;
  // Takes it out and writes it back home, if the Aria wrote to it: an
  // image only looked at is left as it was, a short archive image short.
  // False, with it put back and the user told, when its home could not
  // take it.
  bool Eject(bool quiet);
  void ToggleLock();
  // Every half second from the window's timer.
  void Autosave();
  // Once the emulator thread has stopped. A failure is told.
  void SaveOnExit();
  // The medium the last run ended with, before the machine starts.
  void InsertRemembered();
  // " — karta Dopisy, 1 MB, zamčená" or " — bez karty".
  virtual std::wstring TitlePart() const;

  // The tones, the Eureka emulator's diskette tones note for note: two
  // short notes up for in, one low for out, one long note low for locked
  // and high for unlocked.
  static void ToneInserted();
  static void ToneEjected();
  static void ToneLocked();
  static void ToneUnlocked();

 protected:
  virtual const Texts& texts() const = 0;

  // The machine's side, on the emulator thread. Insert gets a medium the
  // drive is empty for; Take hands back the contents of the one in it.
  virtual void Insert(AriaMachine& machine, std::vector<uint8_t> image, bool locked) = 0;
  virtual std::optional<std::vector<uint8_t>> Take(AriaMachine& machine) = 0;
  // Writes so far and whether anything is unsaved; false when empty.
  virtual bool Writes(AriaMachine& machine, uint64_t& writes, bool& modified) = 0;
  virtual std::vector<uint8_t> Contents(AriaMachine& machine) = 0;
  // Saved: owes nothing, unless written again since `writes`.
  virtual void ClearModified(AriaMachine& machine, uint64_t writes) = 0;
  virtual void SetProtected(AriaMachine& machine, bool locked) = 0;

  // Image files. ReadImage says whether the file itself is read-only, which
  // locks the medium for good.
  virtual bool ReadImage(const std::filesystem::path& file, std::vector<uint8_t>& image, bool& read_only,
                         std::wstring& error) = 0;
  virtual bool WriteImage(const std::filesystem::path& file, const std::vector<uint8_t>& image,
                          std::wstring& error) = 0;
  virtual std::wstring SizeText() const = 0;

  // The settings' side.
  virtual bool KnownLocked(const std::wstring& path) const = 0;
  virtual void RememberLock(const std::wstring& path, bool locked) = 0;
  // The medium in the drive, with its size (a folder card's comes back
  // from here at the next start).
  virtual void RememberMedium(const std::wstring& path, uint32_t size, bool locked) { (void)size; RememberLock(path, locked); }
  virtual std::wstring LastPath() const = 0;
  virtual void SetLastPath(const std::wstring& path) = 0;
  virtual std::wstring ImageFolder() const = 0;
  virtual void SetImageFolder(const std::wstring& folder) = 0;
  virtual std::wstring Slot(int number) const = 0;
  virtual void SetSlot(int number, const std::wstring& path) = 0;

  // A folder named in a slot or remembered from the last run; drives
  // without folder media say so.
  virtual void InsertFolderSlot(const std::wstring& folder, bool locked) = 0;
  virtual bool MountRememberedFolder(const std::wstring& folder, bool locked) = 0;

  bool MountImage(const std::wstring& file, bool locked);
  // Puts it in, the one in the drive going back home first. readOnly: its
  // file may not be written, which locks it for good.
  bool PutIn(std::unique_ptr<FolderCard> folder, const std::wstring& home, std::vector<uint8_t> image, bool locked,
             bool readOnly = false);
  // Takes out the medium in the drive when it is the one at `path`; false
  // when it could not be saved and stayed in.
  bool EjectIfIn(const std::wstring& path);
  // Where a dialog opens: for an image, the folder of the last image; else,
  // or before one, beside the medium in the drive or the last one.
  std::wstring DialogFolder(bool image) const;
  // Records the medium in the settings and saves them; a failure is told.
  void Remember();
  bool Save(const std::vector<uint8_t>& image, std::wstring& error);
  // Takes the medium out of the machine; `written` says whether it owes a
  // save (the Aria wrote to it since the last one).
  std::optional<std::vector<uint8_t>> TakeOut(bool& written);
  void Tell(const std::wstring& text, UINT icon) const;
  // After a change of what is in the drive or its lock.
  void Changed() const;

  EmulatorThread& emulator_;
  Settings& settings_;
  TextEncoding encoding_;
  HWND owner_ = nullptr;
  // Told after every change, to refresh the title.
  std::function<void()> onChange_;

  // The medium in the drive: its home, a folder or an image file, empty
  // when the drive is; and for a folder what writes it back.
  std::filesystem::path home_;
  std::unique_ptr<FolderCard> folder_;
  uint32_t size_ = 0;
  bool locked_ = false;
  // The file is read-only: locked, and the lock cannot be lifted.
  bool readOnly_ = false;
  // The autosave: the write count at the last look, how many looks it has
  // stood still, and whether the last save failed (told once).
  uint64_t writes_ = 0;
  int quietLooks_ = 0;
  bool autosaveFailed_ = false;
  // Put back after a failed save: rebuilt from its bytes, the medium no
  // longer knows it was written, so the drive remembers that it owes one.
  bool owed_ = false;
  // The damaged files last told about, so the autosave does not repeat it.
  std::vector<std::wstring> damagedTold_;
};

#endif
