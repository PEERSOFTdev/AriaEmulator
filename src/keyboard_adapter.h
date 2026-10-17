#ifndef ARIA_KEYBOARD_ADAPTER_H
#define ARIA_KEYBOARD_ADAPTER_H

#include <cstdint>
#include <deque>

#include "usart8251.h"

// The adapter that put an IBM/AT keyboard on the Aria's serial line, as
// ATKBD.COM talks to it (HANDOFF 6.6, ar-w6w): 9600 Bd 8N1; 55h asks
// whether a keyboard is there, answered FAh (yes) or FFh (adapter without
// a keyboard); 10h plus the lock bits sets the keyboard's LEDs, with no
// answer; otherwise it sends the keyboard's scan codes, set 1, release
// with bit 7, E0h before the extended keys.
class KeyboardAdapter : public SerialDevice {
 public:
  // LED bits as ATKBD sends them (ATKBD.COM+0411h): Caps Lock is bit 2.
  static constexpr uint8_t kCapsLock = 0x04;

  void Receive(uint8_t byte) override;
  bool Transmit(uint8_t& byte) override;

  // Whether a keyboard is plugged into the adapter.
  void SetKeyboard(bool present) { keyboard_ = present; }
  // Scan-code bytes, in order, as the keyboard sends them.
  void Send(uint8_t byte) { out_.push_back(byte); }
  // A key that has gone up or down; `extended` puts E0h in front.
  void Key(uint8_t scan, bool extended, bool down);
  uint8_t leds() const { return leds_; }
  // Back to power-on: nothing queued, LEDs off.
  void Reset();

 private:
  bool keyboard_ = true;
  uint8_t leds_ = 0;
  std::deque<uint8_t> out_;
};

#endif
