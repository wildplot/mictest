/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This examples creates a USB Microphone device using the TinyUSB
 * library and captures data from a PDM microphone using a sample
 * rate of 16 kHz, to be sent the to PC.
 * 
 * The USB microphone code is based on the TinyUSB audio_test example.
 * 
 * https://github.com/hathach/tinyusb/tree/master/examples/device/audio_test
 */

/*
 * USB microphone example using ADS1115 over I2C instead of an analog pin.
 *
 * Notes:
 * - ADS1115 cannot reach typical audio sampling rates (kHz). The ADS1115
 *   maximum data rate is 860 SPS; this example caps the rate and is intended
 *   for demonstration only. For real audio rates use a PDM microphone or
 *   a dedicated high-speed ADC.
 */

#include "pico/i2c_microphone.h"
#include "usb_microphone.h"

// configuration
const struct i2c_microphone_config config = {
  .i2c = i2c1, // Challenger RP2040 SDA/SCL header: GPIO 2/3, I2C1
  .i2c_addr = 0x48,
  .i2c_sda = 2, // For the XAIO - might be 4 and 5
  .i2c_scl = 3,
  .sample_rate = 860,
  .sample_buffer_size = 1,
};

static volatile uint16_t latest_adc_sample;
static uint16_t usb_sample_buffer[SAMPLE_BUFFER_SIZE];

// callback functions
void on_i2c_samples_ready();
void on_usb_microphone_tx_ready();

int main(void)
{
  // Initialize the ADS1115 and use its maximum continuous conversion rate.
  i2c_microphone_init(&config);
  int ads_configured = i2c_microphone_configure_ads1115() == 0;
  i2c_microphone_set_samples_ready_handler(on_i2c_samples_ready);

  // initialize the USB microphone interface
  usb_microphone_init();
  usb_microphone_set_tx_ready_handler(on_usb_microphone_tx_ready);
  if (ads_configured) {
    i2c_microphone_start();
  }

  while (1) {
    usb_microphone_task();
  }

  return 0;
}

void on_i2c_samples_ready()
{
  uint16_t sample;
  i2c_microphone_read(&sample, 1);
  latest_adc_sample = sample;
}

void on_usb_microphone_tx_ready()
{
  // Hold each 860 SPS ADC reading across the 8 kHz USB audio frames.
  uint16_t sample = latest_adc_sample;
  for (size_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
    usb_sample_buffer[i] = sample;
  }
  usb_microphone_write(usb_sample_buffer, sizeof(usb_sample_buffer));
}

/*
#include "pico/stdlib.h"
#include "pico/analog_microphone.h"
#include "usb_microphone.h"

// The Adafruit electret microphone amplifier (MAX9814) has a bias voltage of 1.25V with a 2Vpp 
// output on the amplifier's output. The amplifier typically operates with a supply voltage of 2.7V to 5.5V.
// The output signal has a DC bias of VCC/2, meaning if you are using a 3.3V supply, the bias voltage will be 1.65V. 
// If you need to couple the output to other audio equipment that requires AC coupling, a 100uF capacitor 
// should be placed in series. 

const struct analog_microphone_config config = {
    // GPIO to use for input, must be ADC compatible (GPIO 26 - 29)
    // On the Feather these correspond to A0 - A4 (Or ADC0 to ADC3)
    .gpio = 26,
    // bias voltage of microphone in volts
    .bias_voltage = 1.65,
    // sample rate in Hz
    .sample_rate = SAMPLE_RATE,
    // number of samples to buffer
    .sample_buffer_size = SAMPLE_BUFFER_SIZE,
};

// variables
uint16_t sample_buffer[SAMPLE_BUFFER_SIZE];

// callback functions
void on_usb_microphone_tx_ready()
{
  // Callback from TinyUSB library when all data is ready
  // to be transmitted.
  //
  // Write local buffer to the USB microphone
  usb_microphone_write(sample_buffer, sizeof(sample_buffer));
}

void on_analog_samples_ready()
{
    analog_microphone_read(sample_buffer, SAMPLE_BUFFER_SIZE);
}

int main(void)
{
  // initialize and start the PDM microphone
  analog_microphone_init(&config);
  analog_microphone_set_samples_ready_handler(on_analog_samples_ready);
  analog_microphone_start();

  // initialize the USB microphone interface
  usb_microphone_init();
  usb_microphone_set_tx_ready_handler(on_usb_microphone_tx_ready);

  while (1) {
    // run the USB microphone task continuously
    usb_microphone_task();
  }

  return 0;
}
*/
