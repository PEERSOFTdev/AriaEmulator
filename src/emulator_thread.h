#ifndef ARIA_EMULATOR_THREAD_H
#define ARIA_EMULATOR_THREAD_H

// The machine runs on its own thread and the window only talks to it, as
// in the Eureka A4 emulator: a menu, a modal dialog or a window being
// dragged spins its own message loop inside Windows, and a main loop that
// carried the emulator would freeze with it -- and the speech with it, which
// on this machine is the whole user interface.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "aria_machine.h"

// Posted to the window when the machine switches off (wParam 1) or on
// (wParam 0): by itself through 0701h, or by a command below.
constexpr UINT WM_ARIA_POWER = WM_APP + 2;

class EmulatorThread {
 public:
  explicit EmulatorThread(std::unique_ptr<AriaMachine> machine);
  ~EmulatorThread();

  // Where WM_ARIA_POWER goes. Before Start.
  void SetNotifyWindow(HWND window) { notify_ = window; }
  void Start();
  void Stop();

  // The machine, for the host once Stop has returned (to save what the
  // battery keeps). Never while the thread runs.
  AriaMachine& machine() { return *machine_; }

  // Power commands from the window, carried out on the emulator thread.
  // PostPowerOn switches an off machine on and resets a running one, with
  // RAM kept (the warm reset); PostBatteryCut empties RAM and CMOS first.
  void PostPowerOn() { command_ = kCommandPowerOn; }
  void PostBatteryCut() { command_ = kCommandBatteryCut; }
  void PostPowerOff() { command_ = kCommandPowerOff; }

  // False when no audio device could be opened; the machine runs anyway.
  bool AudioOk() const { return audio_ok_.load(); }

  // A key of the Aria's matrix went down or up: `row` is 0, 1 or 2 for
  // ports 0581h, 0582h, 0584h, `bits` the key's bits in that row
  // (HANDOFF 6.4). Called from the window thread.
  void Key(int row, uint8_t bits, bool down);

 private:
  struct KeyEvent {
    int row;
    uint8_t bits;
    bool down;
  };

  enum Command { kCommandNone, kCommandPowerOn, kCommandBatteryCut, kCommandPowerOff };

  void Loop();
  // Carries out a pending power command.
  void ApplyCommand();
  // Forgets every key, held or queued: a machine that starts again has
  // nothing down.
  void ClearKeys();
  void Notify(bool off) const;
  // Applies queued key events. A release waits until the latest press is
  // kMinHoldCycles old in emulated time, so that BKB's periodic scan takes
  // even the quickest chord whole.
  void ApplyKeys(int64_t now);

  std::unique_ptr<AriaMachine> machine_;
  std::mutex keys_mutex_;
  std::deque<KeyEvent> keys_;
  uint8_t rows_[3] = {};
  int64_t last_press_ = 0;
  std::thread thread_;
  HWND notify_ = nullptr;
  std::atomic<int> command_{kCommandNone};
  bool off_ = false;  // As last told to the window.
  std::atomic<bool> running_{false};
  std::atomic<bool> audio_ok_{true};
};

#endif
