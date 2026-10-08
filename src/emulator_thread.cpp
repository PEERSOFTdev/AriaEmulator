#include "emulator_thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <chrono>

#include "audio_player.h"

namespace {

// The audio latency the loop steers to. A little above the Eureka's 22 ms:
// the Aria's DSP delivers its samples in bursts of a SPORT buffer (102
// samples, about 3 ms at 32 kHz) and the CPU runs in slices.
constexpr double kTargetLatencyMs = 30.0;

// BKB scans the matrix in timer 2's interrupt, whose maximum AAPI sets to
// C925h (AAPI.EXE+9F20); timer 2 counts a quarter of the CPU clock.
constexpr int64_t kScanCycles = 0xC925 * 4;

// A chord counts only once the whole matrix has stayed unchanged for three
// scans after the last change (BKB.COM+0350h sets [149h] = 3, +031Ah counts
// it down, +03AFh then takes the chord). So no key is released before the
// latest press is five scans old: one for the scan to notice the change,
// three to count, one to spare for the phase of the tick.
constexpr int64_t kMinHoldCycles = kScanCycles * 5;

}  // namespace

void EmulatorThread::Key(int row, uint8_t bits, bool down) {
  std::lock_guard<std::mutex> lock(keys_mutex_);
  keys_.push_back({row, bits, down});
}

void EmulatorThread::ApplyKeys(int64_t now) {
  std::lock_guard<std::mutex> lock(keys_mutex_);
  while (!keys_.empty()) {
    const KeyEvent event = keys_.front();
    if (event.row < 0 || event.row > 2) {
      keys_.pop_front();
      continue;
    }
    if (event.down) {
      rows_[event.row] |= event.bits;
      last_press_ = now;
    } else {
      // Keep order: a release that must still wait holds back what follows.
      // Counting from the latest press of any key keeps a quick chord, whose
      // keys go down one after another, together long enough for BKB.
      if (now - last_press_ < kMinHoldCycles) return;
      rows_[event.row] &= static_cast<uint8_t>(~event.bits);
    }
    machine_->SetKeyRow(event.row, rows_[event.row]);
    keys_.pop_front();
  }
}

EmulatorThread::EmulatorThread(std::unique_ptr<AriaMachine> machine) : machine_(std::move(machine)) {}

EmulatorThread::~EmulatorThread() { Stop(); }

void EmulatorThread::Start() {
  if (running_.exchange(true)) return;
  thread_ = std::thread([this] { Loop(); });
}

void EmulatorThread::Stop() {
  if (!running_.exchange(false)) return;
  if (thread_.joinable()) thread_.join();
}

void EmulatorThread::Loop() {
  using Clock = std::chrono::steady_clock;
  timeBeginPeriod(1);
  AudioPlayer audio;
  audio_ok_ = audio.Open(AriaMachine::kAudioHz);

  AriaMachine& machine = *machine_;
  machine.KeepSamples(false);
  machine.PowerOn();

  auto last = Clock::now();
  long double guest_clock = 0;   // CPU cycles the wall clock has earned.
  long double done = 0;          // CPU cycles run.
  while (running_.load()) {
    const auto now = Clock::now();
    double delta = std::chrono::duration<double>(now - last).count();
    last = now;
    // A scheduling stall is the host's problem, not the guest's.
    if (delta > 0.25) delta = 0.25;

    // Slewing the emulated clock by at most two percent holds the audio
    // queue at its target without dropping or repeating samples; two
    // percent is a third of a semitone (Eureka emulator_thread.cpp).
    double rate = static_cast<double>(AriaMachine::kCpuHz);
    if (audio.Ready()) {
      const double error = audio.QueuedMs() - kTargetLatencyMs;
      rate *= 1.0 - std::clamp(error * 0.002, -0.02, 0.02);
    }
    guest_clock += static_cast<long double>(rate) * delta;

    ApplyKeys(static_cast<int64_t>(done));
    const long double owed = guest_clock - done;
    if (owed >= 1) {
      const int cycles = static_cast<int>(std::min<long double>(owed, AriaMachine::kCpuHz / 4));
      machine.Run(cycles);
      done += cycles;
    }
    // If the host could not keep up, forgive the debt rather than sprint
    // through the speech to catch up.
    if (guest_clock > done + AriaMachine::kCpuHz / 4.0L) guest_clock = done + AriaMachine::kCpuHz / 4.0L;

    audio.Submit(machine.TakeAudio());
    Sleep(2);
  }
  audio.Close();
  timeEndPeriod(1);
}
