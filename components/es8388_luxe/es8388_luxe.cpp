#include "es8388_luxe.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cmath>

#include <soc/io_mux_reg.h>

namespace esphome::es8388_luxe {

static const char *const TAG = "es8388_luxe";

#define ES8388_WRITE(reg, value) \
  if (!this->write_byte((reg), (value))) { \
    ESP_LOGE(TAG, "Write to register 0x%02X failed", (reg)); \
    return false; \
  }

void ES8388Luxe::setup() {
  // The Luxe clocks the codec from GPIO0. Route the internal clock out to that pad
  // so the codec has an MCLK while it is being configured: the I2S peripheral only
  // drives its own MCLK once a stream starts, which is long after this runs.
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
  WRITE_PERI_REG(PIN_CTRL, READ_PERI_REG(PIN_CTRL) & 0xFFFFFFF0);

  if (!this->configure_()) {
    this->mark_failed();
  }
}

void ES8388Luxe::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8388 (Muse Luxe):");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup failed");
    return;
  }
  ESP_LOGCONFIG(TAG, "  Output route: %s",
                this->route_ == OUTPUT_ROUTE_SPEAKER ? "internal speaker" : "headphone jack");
}

bool ES8388Luxe::configure_() {
  // Reset
  ES8388_WRITE(ES8388_CONTROL1, 0x80);
  ES8388_WRITE(ES8388_CONTROL1, 0x00);
  // Mute while we reconfigure
  ES8388_WRITE(ES8388_DACCONTROL3, ES8388_DACCONTROL3_DAC_MUTE);
  // Power up, I2S secondary ("slave") mode
  ES8388_WRITE(ES8388_CONTROL2, 0x50);
  ES8388_WRITE(ES8388_CHIPPOWER, 0x00);
  ES8388_WRITE(ES8388_MASTERMODE, 0x00);
  // Analogue outputs off until the route is applied below
  ES8388_WRITE(ES8388_DACPOWER, ES8388_DAC_OUTPUT_NONE);
  // vmidsel 500k, play and record
  ES8388_WRITE(ES8388_CONTROL1, 0x12);

  // DAC: 16 bit I2S, MCLK = 256 x Fs
  ES8388_WRITE(ES8388_DACCONTROL1, 0x18);
  ES8388_WRITE(ES8388_DACCONTROL2, 0x02);
  // Mixer fed from LIN2/RIN2, left DAC to left mixer, right DAC to right mixer, 0 dB
  ES8388_WRITE(ES8388_DACCONTROL16, 0x00);
  ES8388_WRITE(ES8388_DACCONTROL17, 0x90);
  ES8388_WRITE(ES8388_DACCONTROL20, 0x90);
  // ADC and DAC share one LRCK
  ES8388_WRITE(ES8388_DACCONTROL21, 0x80);
  ES8388_WRITE(ES8388_DACCONTROL23, 0x00);

  // ADC: power down while it is configured
  ES8388_WRITE(ES8388_ADCPOWER, 0xFF);
  // +24 dB microphone boost. The Luxe's analogue mic needs this; note that the ALC
  // is deliberately left disabled, it pumps badly on top of this much boost.
  ES8388_WRITE(ES8388_ADCCONTROL1, 0x88);
  // LINPUT1/RINPUT1
  ES8388_WRITE(ES8388_ADCCONTROL2, 0x00);
  // 16 bit I2S, MCLK = 256 x Fs
  ES8388_WRITE(ES8388_ADCCONTROL4, 0x0C);
  ES8388_WRITE(ES8388_ADCCONTROL5, 0x02);
  // High pass filter on, 0 dB digital ADC volume
  ES8388_WRITE(ES8388_ADCCONTROL6, 0x30);
  ES8388_WRITE(ES8388_ADCCONTROL8, 0x00);
  ES8388_WRITE(ES8388_ADCCONTROL9, 0x00);
  // Noise gate at -76.5 dB
  ES8388_WRITE(ES8388_ADCCONTROL14, 0x03);
  // ADC on
  ES8388_WRITE(ES8388_ADCPOWER, 0x00);

  // Kick the internal state machine
  ES8388_WRITE(ES8388_CHIPPOWER, 0xF0);
  delay(1);
  ES8388_WRITE(ES8388_CHIPPOWER, 0x00);

  // Output stage level, applied to both routes
  ES8388_WRITE(ES8388_DACCONTROL24, ES8388_OUTPUT_VOLUME);
  ES8388_WRITE(ES8388_DACCONTROL25, ES8388_OUTPUT_VOLUME);
  ES8388_WRITE(ES8388_DACCONTROL26, ES8388_OUTPUT_VOLUME);
  ES8388_WRITE(ES8388_DACCONTROL27, ES8388_OUTPUT_VOLUME);

  if (!this->set_output_route(this->route_))
    return false;
  if (!this->apply_volume_())
    return false;
  return this->set_mute_state_(this->is_muted_);
}

bool ES8388Luxe::reinitialize() {
  if (!this->configure_()) {
    ESP_LOGE(TAG, "Re-initialization failed");
    return false;
  }
  ESP_LOGD(TAG, "Re-initialized");
  return true;
}

bool ES8388Luxe::set_output_route(OutputRoute route) {
  this->route_ = route;
  ESP_LOGD(TAG, "Routing output to %s", route == OUTPUT_ROUTE_SPEAKER ? "internal speaker" : "headphone jack");
  // Only the output power register is touched here. The stock firmware's jack script
  // also wrote DACCONTROL6 and DACCONTROL7, but it only ran on a jack state change,
  // so on a device that never sees a headphone plug both stayed at their reset
  // values - which is also what Raspiaudio's current music firmware leaves them at.
  // Writing them unconditionally put the two drivers out of polarity with each
  // other, cancelling on axis and taking the bass with it.
  ES8388_WRITE(ES8388_DACPOWER,
               route == OUTPUT_ROUTE_SPEAKER ? ES8388_DAC_OUTPUT_LOUT1_ROUT1 : ES8388_DAC_OUTPUT_LOUT2_ROUT2);
  return true;
}

bool ES8388Luxe::set_volume(float volume) {
  this->volume_ = clamp(volume, 0.0f, 1.0f);
  return this->apply_volume_();
}

bool ES8388Luxe::apply_volume_() {
  uint8_t value = ES8388_DAC_VOLUME_SILENT_REG;
  if (this->volume_ > 0.0f) {
    // Attenuation in dB, then in the register's 0.5 dB steps.
    const float attenuation_db = -ES8388_DAC_VOLUME_MIN_DB * (1.0f - this->volume_);
    value = static_cast<uint8_t>(lroundf(attenuation_db * 2.0f));
  }
  ESP_LOGD(TAG, "Setting DAC volume to 0x%02X (%.2f)", value, this->volume_);
  ES8388_WRITE(ES8388_DACCONTROL4, value);
  ES8388_WRITE(ES8388_DACCONTROL5, value);
  return true;
}

bool ES8388Luxe::set_mute_state_(bool mute_state) {
  uint8_t value = 0;
  if (!this->read_byte(ES8388_DACCONTROL3, &value)) {
    ESP_LOGE(TAG, "Read of register 0x%02X failed", ES8388_DACCONTROL3);
    return false;
  }

  // Only touch the mute bit; the rest of this register holds ramp settings.
  if (mute_state) {
    value |= ES8388_DACCONTROL3_DAC_MUTE;
  } else {
    value &= ~ES8388_DACCONTROL3_DAC_MUTE;
  }

  ES8388_WRITE(ES8388_DACCONTROL3, value);
  this->is_muted_ = mute_state;
  return true;
}

}  // namespace esphome::es8388_luxe
