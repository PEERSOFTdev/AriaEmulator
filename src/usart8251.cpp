#include "usart8251.h"

namespace {

// Command bits (8251 data sheet; FOSSIL.COM+0841h-+08BDh sets them).
constexpr uint8_t kTxEnable = 0x01;
constexpr uint8_t kRxEnable = 0x04;
constexpr uint8_t kErrorReset = 0x10;
constexpr uint8_t kInternalReset = 0x40;

// Status bits. Bit 7 is the DSR input, wired on the Aria so that FOSSIL
// reads 0 as "clear to send" (FOSSIL.COM+08F4h); held there.
constexpr uint8_t kTxReady = 0x01;
constexpr uint8_t kRxReady = 0x02;
constexpr uint8_t kTxEmpty = 0x04;
constexpr uint8_t kOverrun = 0x10;

}  // namespace

void Usart8251::Reset() {
  state_ = State::kMode;
  mode_ = 0;
  command_ = 0;
  rx_ready_ = overrun_ = receiving_ = false;
  tx_full_ = sending_ = false;
  rx_left_ = tx_left_ = 0;
  Update();
}

int64_t Usart8251::ByteCycles() const {
  const int64_t tick = clock_ ? clock_() : 0;
  if (tick <= 0) return 0;
  // Asynchronous mode only; FOSSIL never sets a synchronous one.
  static constexpr int kFactor[4] = {0, 1, 16, 64};
  const int factor = kFactor[mode_ & 0x03];
  if (factor == 0) return 0;
  // In half bits, for 1.5 stop bits: start, data, parity, stop.
  int halves = 2 * (1 + 5 + ((mode_ >> 2) & 0x03));
  if (mode_ & 0x10) halves += 2;
  static constexpr int kStopHalves[4] = {2, 2, 3, 4};
  halves += kStopHalves[mode_ >> 6];
  return tick * factor * halves / 2;
}

uint8_t Usart8251::Read(int port) {
  if (port == 0) {
    rx_ready_ = false;
    Update();
    return rx_data_;
  }
  uint8_t status = 0;
  if (!tx_full_) status |= kTxReady;
  if (rx_ready_) status |= kRxReady;
  if (!tx_full_ && !sending_) status |= kTxEmpty;
  if (overrun_) status |= kOverrun;
  return status;
}

void Usart8251::Write(int port, uint8_t value) {
  if (port == 0) {
    tx_data_ = value;
    tx_full_ = true;
    Update();
    return;
  }
  switch (state_) {
    case State::kMode:
      mode_ = value;
      // Synchronous mode (factor bits 00) takes one or two sync characters
      // next; that is how 00h 00h 00h lands in command state from any
      // state before the reset 40h (FOSSIL.COM+082Bh).
      if ((value & 0x03) == 0) {
        state_ = (value & 0x80) ? State::kSync2 : State::kSync1;
      } else {
        state_ = State::kCommand;
      }
      break;
    case State::kSync1:
      state_ = State::kSync2;
      break;
    case State::kSync2:
      state_ = State::kCommand;
      break;
    case State::kCommand:
      if (value & kInternalReset) {
        state_ = State::kMode;
        command_ = 0;
        break;
      }
      command_ = value;
      if (value & kErrorReset) overrun_ = false;
      break;
  }
  Update();
}

void Usart8251::Advance(int cycles) {
  if (sending_) {
    tx_left_ -= cycles;
    if (tx_left_ <= 0) {
      sending_ = false;
      if (device_) device_->Receive(tx_shift_);
    }
  }
  if (receiving_) {
    rx_left_ -= cycles;
    if (rx_left_ <= 0) {
      receiving_ = false;
      if (rx_ready_) overrun_ = true;
      // Fewer than eight data bits leave the top bits clear.
      rx_data_ = static_cast<uint8_t>(rx_shift_ & (0xFF >> (3 - ((mode_ >> 2) & 0x03))));
      rx_ready_ = true;
    }
  }
  if (!receiving_ && device_ && state_ == State::kCommand && (command_ & kRxEnable)) {
    const int64_t length = ByteCycles();
    uint8_t byte = 0;
    if (length > 0 && device_->Transmit(byte)) {
      rx_shift_ = byte;
      rx_left_ = length;
      receiving_ = true;
    }
  }
  Update();
}

void Usart8251::Update() {
  // The buffer moves to the shift register as soon as it is free, so TxRDY
  // comes back one byte ahead of the line.
  if (!sending_ && tx_full_ && state_ == State::kCommand && (command_ & kTxEnable)) {
    const int64_t length = ByteCycles();
    if (length > 0) {
      tx_shift_ = tx_data_;
      tx_full_ = false;
      sending_ = true;
      tx_left_ = length;
    }
  }
  // The TxRDY pin also needs TxEN (and CTS, held asserted).
  const bool rx_line = rx_ready_;
  const bool tx_line = !tx_full_ && (command_ & kTxEnable) && state_ == State::kCommand;
  if (rx_line != rx_line_ || tx_line != tx_line_) {
    rx_line_ = rx_line;
    tx_line_ = tx_line;
    if (lines_) lines_(rx_line, tx_line);
  }
}
