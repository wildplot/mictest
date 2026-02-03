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
  .i2c = i2c0,
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
