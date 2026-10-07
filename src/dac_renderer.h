#ifndef ARIA_DAC_RENDERER_H
#define ARIA_DAC_RENDERER_H

#include <cstdint>
#include <vector>

// Turns the words the DSP shifts out of SPORT1 into a continuous stream at
// the host's audio rate (HANDOFF 6.3). Each word arrives with the time it
// was sent, in DSP cycles, and the stream follows that clock: between two
// close words it interpolates linearly, and where no word comes for longer
// than `hold_cycles` (the DSP switches SPORT1 off in silence, LPC.DSP
// 010Eh) it gives silence, so the host device never starves.
class DacRenderer {
 public:
  DacRenderer(double input_hz, int output_hz, int64_t hold_cycles);

  // A word that finished at DSP cycle `at` (non-decreasing).
  void Sample(int64_t at, int16_t value);
  // The DSP clock has reached `now`: emit what is certain by then. Output
  // lags `now` by `hold_cycles`, so a word arriving in time can still be
  // interpolated towards.
  void AdvanceTo(int64_t now);
  // The output since the last call.
  std::vector<int16_t> Take();

  int output_hz() const { return output_hz_; }

 private:
  void EmitUntil(double until, bool next_known, int64_t next_at, int16_t next_value);

  double cycles_per_output_;
  int output_hz_;
  int64_t hold_;
  double next_out_ = 0.0;      // DSP time of the next output sample.
  int64_t last_at_ = 0;
  int16_t last_ = 0;
  bool have_last_ = false;
  std::vector<int16_t> out_;
};

#endif
