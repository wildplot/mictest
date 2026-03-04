// Minimal I2C microphone front-end that reads samples from an ADS1115
// into an internal buffer and calls a samples-ready handler.

#include "pico/i2c_microphone.h"
#include "ads1115.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Internal state
static struct i2c_microphone_config cfg;
static ads1115_adc_t ads1115;
static uint16_t *internal_buffer = NULL;
static size_t internal_index = 0;
static void (*samples_ready_handler)(void) = NULL;
static repeating_timer_t sample_timer;
static bool timer_added = false;

void i2c_microphone_set_samples_ready_handler(void (*handler)(void))
{
    samples_ready_handler = handler;
}

void i2c_microphone_init(const struct i2c_microphone_config *config)
{
    // copy config
    memcpy(&cfg, config, sizeof(cfg));

    // init i2c peripheral
    i2c_init(cfg.i2c, 400000); // 100k default; ADS1115 supports up to 400k
    gpio_set_function(cfg.i2c_sda, GPIO_FUNC_I2C);
    gpio_set_function(cfg.i2c_scl, GPIO_FUNC_I2C);
    gpio_pull_up(cfg.i2c_sda);
    gpio_pull_up(cfg.i2c_scl);

    // Give hardware time to stabilize
    sleep_ms(50);

    // Initialize the ADS1115 device using the library
    ads1115_init(cfg.i2c, cfg.i2c_addr, &ads1115);

    // allocate internal buffer
    if (internal_buffer) free(internal_buffer);
    internal_buffer = malloc(sizeof(uint16_t) * cfg.sample_buffer_size);
    internal_index = 0;

    printf("i2c_microphone: Initialized ADS1115 at address 0x%02x\n", cfg.i2c_addr);
}

static bool sample_timer_cb(repeating_timer_t *rt)
{
    (void)rt;
    if (!internal_buffer) return true;

    // Read the latest ADC conversion using the ADS1115 library
    uint16_t sample;
    ads1115_read_adc(&sample, &ads1115);

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
    // Configure ADS1115 for continuous conversion mode at maximum data rate
    // using the ADS1115 library functions for safe register manipulation
    
    if (!ads1115.i2c_port) {
        printf("i2c_microphone: ADS1115 not initialized\n");
        return -1;
    }

    // Set operating mode to continuous conversion
    ads1115_set_operating_mode(ADS1115_MODE_CONTINUOUS, &ads1115);
    printf("i2c_microphone: Set ADS1115 to continuous conversion mode\n");

    // Set data rate to maximum (860 SPS)
    ads1115_set_data_rate(ADS1115_RATE_860_SPS, &ads1115);
    printf("i2c_microphone: Set ADS1115 data rate to 860 SPS\n");

    // Set PGA based on config value (convert from user format to library enum)
    // Assuming cfg.pga is in range 0-5, map to library PGA values
    enum ads1115_pga_t pga_setting;
    switch (cfg.pga) {
        case 0: pga_setting = ADS1115_PGA_6_144; break;
        case 1: pga_setting = ADS1115_PGA_4_096; break;
        case 2: pga_setting = ADS1115_PGA_2_048; break;
        case 3: pga_setting = ADS1115_PGA_1_024; break;
        case 4: pga_setting = ADS1115_PGA_0_512; break;
        case 5: pga_setting = ADS1115_PGA_0_256; break;
        default: pga_setting = ADS1115_PGA_4_096; break;
    }
    ads1115_set_pga(pga_setting, &ads1115);
    printf("i2c_microphone: Set ADS1115 PGA to value %d\n", cfg.pga);

    // Write all configuration changes to the device
    ads1115_write_config(&ads1115);
    
    sleep_ms(10); // Allow time for configuration to take effect
    
    printf("i2c_microphone: ADS1115 configured successfully (continuous, 860 SPS)\n");
    return 0;
}
