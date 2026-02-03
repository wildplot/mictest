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
  .i2c = i2c1,// This is how the XAIO RP2040 board has it wired
  .i2c_addr = 0x48,
  .i2c_sda = 4,
  .i2c_scl = 5,
  .sample_rate = SAMPLE_RATE, // will be capped to ADS1115 max internally
  .sample_buffer_size = SAMPLE_BUFFER_SIZE,
};

// variables
uint16_t sample_buffer[SAMPLE_BUFFER_SIZE];

// callback functions
void on_i2c_samples_ready();
void on_usb_microphone_tx_ready();

int main(void)
{
  // initialize and start the I2C microphone
  i2c_microphone_init(&config);
  // optionally reconfigure ADS1115 for continuous conversions
  i2c_microphone_configure_ads1115();
  i2c_microphone_set_samples_ready_handler(on_i2c_samples_ready);
  i2c_microphone_start();

  // initialize the USB microphone interface
  usb_microphone_init();
  usb_microphone_set_tx_ready_handler(on_usb_microphone_tx_ready);

  while (1) {
    usb_microphone_task();
  }

  return 0;
}

void on_i2c_samples_ready()
{
  // Read new samples into local buffer.
  i2c_microphone_read(sample_buffer, SAMPLE_BUFFER_SIZE);
}

void on_usb_microphone_tx_ready()
{
  // Send samples to USB
  usb_microphone_write(sample_buffer, sizeof(sample_buffer));
}


/* Analogue microphone version

#include "pico/stdlib.h"
#include "pico/analog_microphone.h"
#include "usb_microphone.h"

//The Adafruit electret microphone amplifier (MAX9814) has a bias voltage of 1.25V with a 2Vpp 
//output on the amplifier's output. The amplifier typically operates with a supply voltage of 2.7V to 5.5V.
// The output signal has a DC bias of VCC/2, meaning if you are using a 3.3V supply, the bias voltage will be 1.65V. 
// If you need to couple the output to other audio equipment that requires AC coupling, a 100uF capacitor 
// should be placed in series. 


const struct analog_microphone_config config = {
    // GPIO to use for input, must be ADC compatible (GPIO 26 - 29)
    // On the Feather these correspond to A0 - A4 (Or ADC0 to ADC3)
    .gpio = 26,
    // bias voltage of microphone in volts
    .bias_voltage = 1.65, // <---- SEE COMMENT ABOVE
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


void on_usb_microphone_tx_ready()
{
  // Callback from TinyUSB library when all data is ready
  // to be transmitted.
  //
  // Write local buffer to the USB microphone
  usb_microphone_write(sample_buffer, sizeof(sample_buffer));
}
*/
