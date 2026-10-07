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

}  // namespace

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
