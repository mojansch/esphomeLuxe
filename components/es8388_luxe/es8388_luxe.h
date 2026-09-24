#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

#include "es8388_luxe_const.h"

namespace esphome::es8388_luxe {

/// Which analogue output pair the DAC is routed to. The Muse Luxe wires LOUT1/ROUT1
/// to the internal amplifier and LOUT2/ROUT2 to the headphone jack.
enum OutputRoute : uint8_t {
  OUTPUT_ROUTE_SPEAKER,
  OUTPUT_ROUTE_HEADPHONE,
};

/// ES8388 codec as fitted on the Raspiaudio Muse Luxe.
///
/// ESPHome ships a generic `es8388` audio_dac, but it targets ESP32-LyraT style
/// boards: it never powers up the analogue output stage (it expects a `select` to
/// do that) and it enables the ALC, which fights the Luxe's own microphone boost.
/// This component keeps the register sequence that suits the Luxe while exposing
/// the standard `audio_dac` interface, so the speaker can hand volume and mute to
/// the codec instead of scaling samples in software.
class ES8388Luxe : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // AudioDac interface
  bool set_volume(float volume) override;
  float volume() override { return this->volume_; }
  bool set_mute_off() override { return this->set_mute_state_(false); }
  bool set_mute_on() override { return this->set_mute_state_(true); }
  bool is_muted() override { return this->is_muted_; }

  /// Re-runs the whole register sequence and restores the current volume, mute
  /// and output route. Safe to call at any time; used after the I2S peripheral
  /// has started so the codec is configured with a running MCLK.
  bool reinitialize();

  /// Routes the DAC to the internal amplifier or to the headphone jack.
  bool set_output_route(OutputRoute route);
  OutputRoute output_route() const { return this->route_; }

 protected:
  bool configure_();
  bool apply_volume_();
  bool set_mute_state_(bool mute_state);

  float volume_{1.0f};
  OutputRoute route_{OUTPUT_ROUTE_SPEAKER};
};

}  // namespace esphome::es8388_luxe
