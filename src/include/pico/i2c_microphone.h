// Simple I2C microphone front-end for ADS1115 (example)
#ifndef PICO_I2C_MICROPHONE_H
#define PICO_I2C_MICROPHONE_H

#include <stdint.h>
#include <stddef.h>
#include "hardware/i2c.h"

struct i2c_microphone_config {
    i2c_inst_t *i2c;       // i2c instance, e.g. i2c0
    uint8_t i2c_addr;      // ADS1115 address (default 0x48)
    uint8_t i2c_sda;       // SDA GPIO pin
    uint8_t i2c_scl;       // SCL GPIO pin
    uint32_t sample_rate;  // desired sample rate (SPS). ADS1115 max is 860
    size_t sample_buffer_size; // number of samples per internal buffer
};

// Initialize module (configures I2C peripheral only).
void i2c_microphone_init(const struct i2c_microphone_config *config);

// Start sampling. When an internal buffer is full the samples-ready handler
// will be called (if set).
void i2c_microphone_start(void);

// Stop sampling
void i2c_microphone_stop(void);

// Read `len` samples from the internal buffer into `out`.
// `len` must be <= config->sample_buffer_size and samples are 16-bit unsigned.
void i2c_microphone_read(uint16_t *out, size_t len);

// Set a handler that will be called when the internal sample buffer is ready.
void i2c_microphone_set_samples_ready_handler(void (*handler)(void));

// Helper: configure ADS1115 for continuous conversion. This is optional;
// some systems pre-configure the ADC. This function will attempt to set
// continuous conversion mode using a data rate <= requested sample_rate.
// Returns 0 on success, non-zero on I2C error.
int i2c_microphone_configure_ads1115(void);

#endif // PICO_I2C_MICROPHONE_H
