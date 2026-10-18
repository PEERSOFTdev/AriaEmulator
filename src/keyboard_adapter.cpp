#include "keyboard_adapter.h"

void KeyboardAdapter::Receive(uint8_t byte) {
  if (byte == 0x55) {
    // ATKBD.COM+05DCh sends it and waits about a second; FAh installs the
    // driver, FFh makes it say "klávesnice není připojena." first
    // (+061Dh-+0634h). Whatever was queued would come before the answer.
    out_.clear();
    enabled_ = false;
    out_.push_back(keyboard_ ? 0xFA : 0xFF);
  } else if ((byte & 0xF0) == 0x10) {
    // 10h at install time (+0667h) and 10h + lock bits at every lock key
    // (+041Dh).
    leds_ = byte & 0x07;
    enabled_ = true;
  }
}

bool KeyboardAdapter::Transmit(uint8_t& byte) {
  if (out_.empty()) return false;
  byte = out_.front();
  out_.pop_front();
  return true;
}

void KeyboardAdapter::Key(uint8_t scan, bool extended, bool down) {
  if (!keyboard_ || !enabled_) return;
  if (extended) out_.push_back(0xE0);
  out_.push_back(static_cast<uint8_t>(down ? scan & 0x7F : scan | 0x80));
}

void KeyboardAdapter::Reset() {
  out_.clear();
  leds_ = 0;
  enabled_ = false;
}
