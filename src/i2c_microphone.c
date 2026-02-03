// Minimal I2C microphone front-end that reads samples from an ADS1115
// into an internal buffer and calls a samples-ready handler.

#include "pico/i2c_microphone.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Internal state
static struct i2c_microphone_config cfg;
static uint16_t *internal_buffer = NULL;
static size_t internal_index = 0;
static void (*samples_ready_handler)(void) = NULL;
static repeating_timer_t sample_timer;
static bool timer_added = false;

// ADS1115 registers
#define ADS1115_REG_CONVERSION 0x00
#define ADS1115_REG_CONFIG     0x01

void i2c_microphone_set_samples_ready_handler(void (*handler)(void))
{
    samples_ready_handler = handler;
}

void i2c_microphone_init(const struct i2c_microphone_config *config)
{
    // copy config
    memcpy(&cfg, config, sizeof(cfg));

    // init i2c
    i2c_init(cfg.i2c, 100000); // 100k default; ADS1115 supports up to 400k
    gpio_set_function(cfg.i2c_sda, GPIO_FUNC_I2C);
    gpio_set_function(cfg.i2c_scl, GPIO_FUNC_I2C);
    gpio_pull_up(cfg.i2c_sda);
    gpio_pull_up(cfg.i2c_scl);

    // allocate internal buffer
    if (internal_buffer) free(internal_buffer);
    internal_buffer = malloc(sizeof(uint16_t) * cfg.sample_buffer_size);
    internal_index = 0;

    // Note: this code assumes the ADS1115 is configured for continuous
    // conversion mode. If not, call i2c_microphone_configure_ads1115()
    // after init to set an appropriate configuration for your sampling rate.
}

static bool sample_timer_cb(repeating_timer_t *rt)
{
    (void)rt;
    if (!internal_buffer) return true;

    // Read conversion register (2 bytes)
    uint8_t reg = ADS1115_REG_CONVERSION;
    uint8_t buf[2];
    int ret = i2c_write_blocking(cfg.i2c, cfg.i2c_addr, &reg, 1, true);
    if (ret < 0) return true; // keep timer running
    ret = i2c_read_blocking(cfg.i2c, cfg.i2c_addr, buf, 2, false);
    if (ret < 0) return true;

    // ADS1115 provides signed 16-bit two's complement. For audio pipeline
    // we'll pass raw 16-bit unsigned values (user can interpret as needed).
    uint16_t sample = ((uint16_t)buf[0] << 8) | buf[1];

    internal_buffer[internal_index++] = sample;
    if (internal_index >= cfg.sample_buffer_size) {
        internal_index = 0;
        if (samples_ready_handler) samples_ready_handler();
    }

    return true; // keep timer
}

void i2c_microphone_start(void)
{
    if (!internal_buffer) return;
    if (timer_added) return;

    // Cap sample rate to a reasonable maximum for ADS1115
    uint32_t sps = cfg.sample_rate;
    if (sps == 0) sps = 128;
    if (sps > 860) sps = 860;

    // interval in microseconds (may not be exact for high rates)
    int64_t interval_us = (int64_t)(1000000LL / sps);

    // add repeating timer
    if (!add_repeating_timer_us(-interval_us, sample_timer_cb, NULL, &sample_timer)) {
        timer_added = true;
    } else {
        printf("i2c_microphone: failed to add timer\n");
    }
}

void i2c_microphone_stop(void)
{
    if (timer_added) {
        cancel_repeating_timer(&sample_timer);
        timer_added = false;
    }
}

void i2c_microphone_read(uint16_t *out, size_t len)
{
    if (!internal_buffer || len > cfg.sample_buffer_size) return;
    // Copy latest block; this simple implementation returns the buffer
    // starting at the current write index (i.e., oldest sample first).
    size_t first = internal_index;
    size_t tail = cfg.sample_buffer_size - first;
    if (len <= tail) {
        memcpy(out, &internal_buffer[first], len * sizeof(uint16_t));
    } else {
        memcpy(out, &internal_buffer[first], tail * sizeof(uint16_t));
        memcpy(out + tail, &internal_buffer[0], (len - tail) * sizeof(uint16_t));
    }
}

int i2c_microphone_configure_ads1115(void)
{
    // This helper attempts to set ADS1115 into continuous conversion mode
    // with a data rate not exceeding cfg.sample_rate. The register layout
    // for ADS1115 may vary and a robust implementation should use a
    // dedicated ADS1115 driver. This function provides a simple example.

    if (!cfg.i2c) return -1;

    // Choose a data rate code (0..7) mapping roughly to {8,16,32,64,128,250,475,860}
    uint32_t sps = cfg.sample_rate ? cfg.sample_rate : 128;
    int dr = 4; // default 128
    if (sps >= 860) dr = 7;
    else if (sps >= 475) dr = 6;
    else if (sps >= 250) dr = 5;
    else if (sps >= 128) dr = 4;
    else if (sps >= 64) dr = 3;
    else if (sps >= 32) dr = 2;
    else if (sps >= 16) dr = 1;
    else dr = 0;

    // Build a typical config: AIN0 single-ended, FS=+/-4.096V, continuous mode, chosen DR
    // WARNING: Values below are illustrative. For production use, replace with a
    // tested ADS1115 driver and ensure the bitfields are correct for your ADS1115.
    uint16_t config_val = 0;
    // MUX single-ended AIN0 = 0x4 (bits 14-12)
    config_val |= (0x4 << 12);
    // PGA: 01 => +/-4.096V (bits 11-9) -> use 0x1
    config_val |= (0x1 << 9);
    // MODE: 0 -> continuous
    // DR bits (7-5)
    config_val |= (dr & 0x7) << 5;
    // COMP_QUE disable comparator (bits 1-0 = 11)
    config_val |= 0x3;

    uint8_t out[3];
    out[0] = ADS1115_REG_CONFIG;
    out[1] = (uint8_t)((config_val >> 8) & 0xFF);
    out[2] = (uint8_t)(config_val & 0xFF);

    int ret = i2c_write_blocking(cfg.i2c, cfg.i2c_addr, out, 3, false);
    return (ret >= 0) ? 0 : ret;
}
