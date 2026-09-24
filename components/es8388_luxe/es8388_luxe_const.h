#pragma once

#include <cstdint>

namespace esphome::es8388_luxe {

/* ES8388 register map (see the ES8388 user guide) */
static const uint8_t ES8388_CONTROL1 = 0x00;
static const uint8_t ES8388_CONTROL2 = 0x01;
static const uint8_t ES8388_CHIPPOWER = 0x02;
static const uint8_t ES8388_ADCPOWER = 0x03;
static const uint8_t ES8388_DACPOWER = 0x04;
static const uint8_t ES8388_MASTERMODE = 0x08;

/* ADC */
static const uint8_t ES8388_ADCCONTROL1 = 0x09;   // PGA / mic boost
static const uint8_t ES8388_ADCCONTROL2 = 0x0A;   // input select
static const uint8_t ES8388_ADCCONTROL4 = 0x0C;   // I2S format / word length
static const uint8_t ES8388_ADCCONTROL5 = 0x0D;   // MCLK ratio
static const uint8_t ES8388_ADCCONTROL6 = 0x0E;   // high pass filter
static const uint8_t ES8388_ADCCONTROL8 = 0x10;   // left ADC volume
static const uint8_t ES8388_ADCCONTROL9 = 0x11;   // right ADC volume
static const uint8_t ES8388_ADCCONTROL14 = 0x16;  // noise gate

/* DAC */
static const uint8_t ES8388_DACCONTROL1 = 0x17;   // I2S format / word length
static const uint8_t ES8388_DACCONTROL2 = 0x18;   // MCLK ratio
static const uint8_t ES8388_DACCONTROL3 = 0x19;   // DAC mute / ramp
static const uint8_t ES8388_DACCONTROL4 = 0x1A;   // left DAC volume
static const uint8_t ES8388_DACCONTROL5 = 0x1B;   // right DAC volume
static const uint8_t ES8388_DACCONTROL6 = 0x1C;   // de-emphasis
static const uint8_t ES8388_DACCONTROL7 = 0x1D;   // VROI / mono / attenuation
static const uint8_t ES8388_DACCONTROL16 = 0x26;  // mixer input select
static const uint8_t ES8388_DACCONTROL17 = 0x27;  // left mixer
static const uint8_t ES8388_DACCONTROL20 = 0x2A;  // right mixer
static const uint8_t ES8388_DACCONTROL21 = 0x2B;  // shared LRCK
static const uint8_t ES8388_DACCONTROL23 = 0x2D;  // VROI
static const uint8_t ES8388_DACCONTROL24 = 0x2E;  // LOUT1 volume
static const uint8_t ES8388_DACCONTROL25 = 0x2F;  // ROUT1 volume
static const uint8_t ES8388_DACCONTROL26 = 0x30;  // LOUT2 volume
static const uint8_t ES8388_DACCONTROL27 = 0x31;  // ROUT2 volume

/* Bit / value constants */
static const uint8_t ES8388_DACCONTROL3_DAC_MUTE = 0x04;  // DACMute, bit 2

static const uint8_t ES8388_DAC_OUTPUT_NONE = 0xC0;         // all outputs powered down
static const uint8_t ES8388_DAC_OUTPUT_LOUT1_ROUT1 = 0x30;  // Muse Luxe: internal speaker amp
static const uint8_t ES8388_DAC_OUTPUT_LOUT2_ROUT2 = 0x0C;  // Muse Luxe: headphone jack

/// DAC volume register, in 0.5 dB steps: 0x00 is 0 dB and 0xC0 is -96 dB.
static const uint8_t ES8388_DAC_VOLUME_SILENT_REG = 192;

/// Attenuation at volume 0, in dB. ESPHome's software volume control spreads a
/// speaker's 0..1 range over -49 dB..0 dB, so matching it here keeps the volume
/// curve the same as it is on a board whose codec does not do volume in hardware.
/// Mapping over the chip's full -96 dB range instead makes every setting roughly
/// 19 dB quieter, which reads as a complete loss of bass long before it reads as
/// a loss of level.
static const float ES8388_DAC_VOLUME_MIN_DB = -49.0f;

/// Output stage volume used for both routes. 0x1E is 0 dB, each step is 1 dB;
/// 0x21 gives the Muse Luxe's amplifier a little headroom above unity.
static const uint8_t ES8388_OUTPUT_VOLUME = 0x21;

}  // namespace esphome::es8388_luxe
