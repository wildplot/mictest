/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#ifndef _USB_MICROPHONE_H_
#define _USB_MICROPHONE_H_

#include "tusb.h"

#ifndef SAMPLE_RATE
#define SAMPLE_RATE ((CFG_TUD_AUDIO_EP_SZ_IN / 2) - 1) * 1000
#endif

#ifndef SAMPLE_BUFFER_SIZE
#define SAMPLE_BUFFER_SIZE ((CFG_TUD_AUDIO_EP_SZ_IN/2) - 1)
#endif

#ifndef SAMPLE_GAIN
#define SAMPLE_GAIN 5 // PGA gain setting for i2c microphone - (0-5): 0=6/1, 1=3/1, 2=2/1, 3=1/1, 4=1/2, 5=1/4. Default 1 for 4.096V
#endif

typedef void (*usb_microphone_tx_ready_handler_t)(void);

void usb_microphone_init();
void usb_microphone_set_tx_ready_handler(usb_microphone_tx_ready_handler_t handler);
void usb_microphone_task();
uint16_t usb_microphone_write(const void * data, uint16_t len);

#endif
