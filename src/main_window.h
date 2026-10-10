#ifndef ARIA_MAIN_WINDOW_H
#define ARIA_MAIN_WINDOW_H

#include <memory>
#include <string>
#include <vector>

#include "card_folder.h"
#include "emulator_thread.h"
#include "settings.h"
#include "text_codec.h"
#include "win/window.h"

// What the window says about the machine it runs: for the title and for
// Nápověda -> O programu.
struct MachineInfo {
  std::wstring version;   // Firmware build, e.g. "7.05".
  std::wstring encoding;  // Of drive B:, e.g. "Latin 2".
  TextEncoding text_encoding = TextEncoding::kUnknown;  // The same, for card names.
  std::wstring flash;     // Path of drive B:'s image.
  std::wstring kernel;    // Path of KERNEL.SYS.
};

// The emulator's window, after the Eureka A4 emulator's MainWindow: the same
// F12 menu, F11 one-shot and Shift+F11 release, with the same tones, so that
// a user of one finds the other where they expect it.  It owns no machine
// state: keys go to the emulator thread, and the client area stays empty,
// because the machine's output is speech.
class MainWindow : public win::Window {
 public:
  MainWindow(EmulatorThread& emulator, MachineInfo info, Settings& settings);

  bool Create();
  HACCEL accelerators() const { return accelerators_; }
  HACCEL hostAccelerators() const { return hostAccelerators_; }

  // True while the keyboard belongs to the host, so the Ctrl shortcuts apply.
  bool HostShortcutsActive() const { return released_ || passOnce_; }

  // Whether what the battery keeps is to be saved when the program ends:
  // the machine was off when the window closed, or the user chose to switch
  // it off on the way out.  Closing a running Aria otherwise is the battery
  // cut, as in the Eureka emulator.
  bool SaveMemoryOnExit() const { return poweredOff_ || switchOffOnExit_; }

  // Takes the card out and writes it back into its folder, once the
  // emulator thread has stopped. A failure is told in a message box.
  void SaveCardOnExit();
  // Puts back the card that was in the slot when the last run ended, with
  // its size and lock, before the machine starts. Told only when it cannot.
  void InsertRememberedCard();

 protected:
  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) override;

 private:
  void RegisterCommands();
  void RefreshTitle() const;
  void RefreshMenu() const;
  // Puts the "F11, " prefix on the Ctrl shortcuts, or takes it off once the
  // keyboard is released and they no longer need it.
  void RefreshShortcutText(HMENU menu) const;
  // A host key went down or up while the Aria has the keyboard.
  void ForwardKey(bool down, WPARAM virtualKey, LPARAM lParam);
  // Lets go of every Aria key the host holds: the releases of those keys will
  // go to whoever has the keyboard next, never to the machine.
  void ReleaseAll();
  // True when this key event belongs to the host rather than to the Aria.
  // Also spends the one-shot, which is why it is not const.
  bool HostKeepsKey(bool down);
  // quiet is for the power switch, whose own tone already says it: off and
  // released are one state (Eureka's MainWindow::SetReleased).
  void SetReleased(bool released, bool quiet = false);
  // Asked on WM_CLOSE while the machine runs. False means the user chose to
  // stay; otherwise switchOffOnExit_ holds whether to keep the memory.
  bool ConfirmClose();
  void SetPassOnce(bool armed);
  std::wstring AboutText() const;

  // The card slot (HANDOFF 6.8, ar-aj1).
  void InsertCardFromFolder();
  void InsertCardFromImage();
  // Karta -> Vložit novou kartu: an empty folder, or an image formatted or
  // not, made and put in.
  void InsertNewCard();
  // Builds the card from `folder`, or reads it from the image `file`, and
  // puts it in, the one in the slot going back to its home first. False,
  // with the user told, when it cannot.
  bool MountFolderCard(const std::wstring& folder, uint32_t size, bool locked);
  bool MountImageCard(const std::wstring& file, bool locked);
  // Takes out the card in the slot when it is the one at `path`; false when
  // it could not be saved and stayed in.
  bool EjectIfInSlot(const std::wstring& path);
  // Where a card dialog opens: for an image, the folder of the last image;
  // else, or before one, beside the card in the slot or the last card.
  // Empty leaves it to the shell.
  std::wstring CardDialogFolder(bool image) const;
  bool PutCardIn(std::unique_ptr<FolderCard> folder, const std::wstring& home, std::vector<uint8_t> image,
                 bool locked);
  // Writes what the card holds now back into its folder while it stays in:
  // the autosave after a second of quiet.
  void AutosaveCard();
  // Records the card in the settings and saves them; a failure is told.
  void RememberCard();
  // Writes `image` back into the card's image file or folder; files a
  // folder card could not take are told (once per set) and the image kept
  // for a look.
  bool SaveCard(const std::vector<uint8_t>& image, std::wstring& error);
  // Takes the card out and writes it back into its folder. False, with the
  // card put back in and the user told, when the folder could not take it.
  bool EjectCard(bool quiet);
  void ToggleCardLock();

  EmulatorThread& emulator_;
  MachineInfo info_;
  Settings& settings_;
  HACCEL accelerators_ = nullptr;
  HACCEL hostAccelerators_ = nullptr;

  // Shift+F11: the keyboard is let go of altogether and the window behaves
  // like any other Windows window until it is taken back.
  bool released_ = false;
  // F11: the next key goes to Windows instead of to the Aria, once.
  bool passOnce_ = false;
  // Whether the press of that key has been seen yet: the release of F11
  // itself must not spend it (see HostKeepsKey).
  bool passOnceUsed_ = false;
  // No sound device could be opened; reported once, kept in the title.
  bool noAudio_ = false;
  // The machine is switched off and sits there with the window open. The
  // emulator thread's state is the truth; this copy builds title and menu.
  bool poweredOff_ = false;
  // Chosen in the close question: switch off, keep the memory.
  bool switchOffOnExit_ = false;
  // Host keys down on the Aria, so that they can all be let go of.
  std::vector<WPARAM> held_;
  // The card in the slot: its home, a folder or an image file, empty when
  // the slot is; and for a folder card what writes it back.
  std::filesystem::path cardHome_;
  std::unique_ptr<FolderCard> cardFolder_;
  uint32_t cardSize_ = 0;
  bool cardLocked_ = false;
  // The autosave: the card's write count at the last look, how many looks
  // it has stood still, and whether the last save failed (told once).
  uint64_t cardWrites_ = 0;
  int cardQuietLooks_ = 0;
  bool autosaveFailed_ = false;
  // The damaged files last told about, so the autosave does not repeat it.
  std::vector<std::wstring> damagedTold_;
};

#endif
