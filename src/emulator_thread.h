#ifndef ARIA_EMULATOR_THREAD_H
#define ARIA_EMULATOR_THREAD_H

// The machine runs on its own thread and the window only talks to it, as
// in the Eureka A4 emulator: a menu, a modal dialog or a window being
// dragged spins its own message loop inside Windows, and a main loop that
// carried the emulator would freeze with it -- and the speech with it, which
// on this machine is the whole user interface.

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "aria_machine.h"

class EmulatorThread {
 public:
  explicit EmulatorThread(std::unique_ptr<AriaMachine> machine);
  ~EmulatorThread();

  void Start();
  void Stop();

  // False when no audio device could be opened; the machine runs anyway.
  bool AudioOk() const { return audio_ok_.load(); }

 private:
  void Loop();

  std::unique_ptr<AriaMachine> machine_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> audio_ok_{true};
};

#endif
