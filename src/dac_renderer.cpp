#include "dac_renderer.h"

#include <utility>

DacRenderer::DacRenderer(double input_hz, int output_hz, int64_t hold_cycles)
    : cycles_per_output_(input_hz / output_hz), output_hz_(output_hz), hold_(hold_cycles) {}

void DacRenderer::EmitUntil(double until, bool next_known, int64_t next_at, int16_t next_value) {
  for (; next_out_ < until; next_out_ += cycles_per_output_) {
    int16_t value = 0;
    if (have_last_ && next_out_ < static_cast<double>(last_at_ + hold_)) {
      value = last_;
      if (next_known && next_at - last_at_ <= hold_ && next_at > last_at_ && next_out_ >= last_at_) {
        const double f = (next_out_ - static_cast<double>(last_at_)) / static_cast<double>(next_at - last_at_);
        value = static_cast<int16_t>(last_ + f * (next_value - last_));
      }
    }
    out_.push_back(value);
  }
}

void DacRenderer::Sample(int64_t at, int16_t value) {
  EmitUntil(static_cast<double>(at), true, at, value);
  last_at_ = at;
  last_ = value;
  have_last_ = true;
}

void DacRenderer::AdvanceTo(int64_t now) {
  EmitUntil(static_cast<double>(now - hold_), false, 0, 0);
}

std::vector<int16_t> DacRenderer::Take() {
  std::vector<int16_t> taken;
  taken.swap(out_);
  return taken;
}
