#ifndef ARIA_MAIN_WINDOW_H
#define ARIA_MAIN_WINDOW_H

#include <string>
#include <vector>

#include "emulator_thread.h"
#include "win/window.h"

// What the window says about the machine it runs: for the title and for
// Nápověda -> O programu.
struct MachineInfo {
  std::wstring version;   // Firmware build, e.g. "7.05".
  std::wstring encoding;  // Of drive B:, e.g. "Latin 2".
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
  MainWindow(EmulatorThread& emulator, MachineInfo info);

  bool Create();
  HACCEL accelerators() const { return accelerators_; }
  HACCEL hostAccelerators() const { return hostAccelerators_; }

  // True while the keyboard belongs to the host, so the Ctrl shortcuts apply.
  bool HostShortcutsActive() const { return released_ || passOnce_; }

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
  void SetReleased(bool released);
  void SetPassOnce(bool armed);
  std::wstring AboutText() const;

  EmulatorThread& emulator_;
  MachineInfo info_;
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
  // Host keys down on the Aria, so that they can all be let go of.
  std::vector<WPARAM> held_;
};

#endif
