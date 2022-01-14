/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Jochen Schaeuble, 2020 Reinhard Panhuber
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/*
 * Based on the USB Microphone example from ArmDeveloperEcosystem
 * https://github.com/ArmDeveloperEcosystem/microphone-library-for-pico/tree/main/examples/usb_microphone
 * and the CDC dual port example
 * https://github.com/hathach/tinyusb/tree/master/examples/device/cdc_dual_ports
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

typedef void (*usb_microphone_tx_pre_handler_t)(void);
typedef void (*usb_microphone_tx_post_handler_t)(void);
typedef void (*usb_microphone_volume_handler_t)(uint8_t channel, uint16_t volume, bool mute);

void usb_devices_init();
void usb_microphone_set_tx_pre_handler(usb_microphone_tx_pre_handler_t handler);
void usb_microphone_set_tx_post_handler(usb_microphone_tx_post_handler_t handler);
void usb_microphone_set_volume_handler(usb_microphone_volume_handler_t handler);
void usb_devices_task();
uint16_t usb_microphone_write(const void * data, uint16_t len);

#endif
