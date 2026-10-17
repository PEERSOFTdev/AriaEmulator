#ifndef ARIA_USART8251_H
#define ARIA_USART8251_H

#include <cstdint>
#include <functional>

// What sits on the far end of the Aria's serial line: the external
// keyboard's adapter (HANDOFF 6.6, ar-w6w), later a PC for file transfer
// (ar-jnh).
class SerialDevice {
 public:
  virtual ~SerialDevice() = default;
  // A byte the Aria sent, once its last stop bit is out.
  virtual void Receive(uint8_t byte) = 0;
  // The next byte for the Aria, if the device has one; asked whenever the
  // receiver is free, so a device sends no faster than the line allows.
  virtual bool Transmit(uint8_t& byte) = 0;
};

// The Aria's serial port: a USART of the Intel 8251 kind at I/O 0600h
// (data) and 0601h (mode, command, status), as FOSSIL.COM drives it
// (HANDOFF 6.6): reset by 00h 00h 00h 40h, then a mode byte and commands;
// RxRDY on the CPU's INT0, TxRDY on INT1, both level-triggered; the
// baud clock is timer 1's output (TMROUT1), 16 clocks a bit.
//
// Bytes take the time the line takes: the number of bits the mode byte
// gives times the baud clock. Modem lines are fixed: CTS asserted (status
// bit 7 reads 0; FOSSIL.COM+08F4h reports CTS on 0), DSR likewise.
class Usart8251 {
 public:
  // Cycles of the CPU per baud-clock tick, 0 while the clock is stopped.
  using BaudClock = std::function<int64_t()>;
  // Called whenever RxRDY or TxRDY may have changed.
  using Lines = std::function<void(bool rx_ready, bool tx_ready)>;

  Usart8251(BaudClock clock, Lines lines) : clock_(std::move(clock)), lines_(std::move(lines)) {}

  void Attach(SerialDevice* device) { device_ = device; }
  SerialDevice* attached() const { return device_; }

  // Power-on: waiting for a mode byte, nothing in flight.
  void Reset();
  // `port` 0 = data, 1 = control/status.
  uint8_t Read(int port);
  void Write(int port, uint8_t value);
  // Time passes on the line.
  void Advance(int cycles);

 private:
  enum class State { kMode, kSync1, kSync2, kCommand };

  int64_t ByteCycles() const;
  void Update();

  BaudClock clock_;
  Lines lines_;
  SerialDevice* device_ = nullptr;

  State state_ = State::kMode;
  uint8_t mode_ = 0;
  uint8_t command_ = 0;
  // Receiver: the holding register and the byte on its way in.
  uint8_t rx_data_ = 0;
  bool rx_ready_ = false;
  bool overrun_ = false;
  bool receiving_ = false;
  uint8_t rx_shift_ = 0;
  int64_t rx_left_ = 0;
  // Transmitter: the buffer the CPU writes and the byte going out.
  uint8_t tx_data_ = 0;
  bool tx_full_ = false;
  bool sending_ = false;
  uint8_t tx_shift_ = 0;
  int64_t tx_left_ = 0;
  bool rx_line_ = false;
  bool tx_line_ = false;
};

#endif
