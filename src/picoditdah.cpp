/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Jochen Schaeuble
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../button-debouncer/button_debounce.h"
#include "cw_generator.h"
#include "winkeyer_parser.h"
#include "pico/malloc.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "usb_devices.h"

CWGenerator *cwgen;
WinKeyerParser *wkparser;

void on_usb_microphone_tx_pre() {
    // write the prepared audio buffer to USB
    usb_microphone_write(cwgen->get_audio_buffer(), cwgen->get_audio_buffer_size());
}

void on_usb_microphone_tx_post() {
    // calculate next buffer
    cwgen->update_statemachine();
}

void on_usb_microphone_volume(uint8_t channel, uint16_t volume, bool mute) {
    if (channel == 0) {
        cwgen->set_volume(volume);
    }
}


/*
 * check serial port for new messages and parse them accordingly
 */
static void cdc_task(void) {
    if (tud_cdc_n_available(0) > 0) {
        uint8_t buf[64];
        uint32_t count = tud_cdc_n_read(0, buf, sizeof(buf));

        // interpret message as WinKeyer message
        count = wkparser->parse_message(buf, count, 64);

        if (count > 0) {
            tud_cdc_n_write(0, buf, count);
            tud_cdc_n_write_flush(0);
        }
    }
}

int main() {
    stdio_init_all();

    printf("PicoDitDah v0.1\n");
    cwgen = new CWGenerator(SAMPLE_RATE, SAMPLE_BUFFER_SIZE);
    wkparser = new WinKeyerParser(cwgen);

    printf("audio_buffer_size: %u\n", cwgen->get_audio_buffer_size());

    usb_devices_init();
    usb_microphone_set_tx_pre_handler(on_usb_microphone_tx_pre);
    usb_microphone_set_tx_post_handler(on_usb_microphone_tx_post);
    usb_microphone_set_volume_handler(on_usb_microphone_volume);

    while (1) {
        // run the USB microphone task continuously
        usb_devices_task();
        cdc_task();
    }
}