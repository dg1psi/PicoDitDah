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

#include "cw_generator.h"

#include "../button-debouncer/button_debounce.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

/*
 * class that generates and audio buffer that contains morse code signals.
 */

#define DIT_GPIO 3                  // GPIO port for the DIT paddle
#define DIT_UNITS 1                 // number of time units for a DIT
#define DAH_GPIO 4                  // GPIO port for the DAH paddle
#define DAH_UNITS 3                 // number of time units for a DAH
#define INTRA_CHAR_PAUSE_UNITS 1    // number of time units for a pause within a characters
#define INTER_CHAR_PAUSE_UNITS 3    // number of time units for a pause between characters
#define INT_WORD_PAUSE_UNITS 7      // number of time units for a pause between words

#define DEFAULT_FREQUENCY 700       // default frequency for the audio tone
#define DEFAULT_WPM 20              // default speed for the morse code in WPM (Words Per Minute)
#define DEFAULT_VOLUME 100          // default volume [%] of the morse signal

// NeoPixel (WS2812) configuration
#define IS_RGBW true
#ifdef PICO_DEFAULT_WS2812_PIN
    #define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
    // default to pin 2 if the board doesn't have a default WS2812 pin defined
    #define WS2812_PIN 2
#endif

#ifdef PICO_DEFAULT_WS2812_POWER_PIN
    #define WS2812_POWER_PIN PICO_DEFAULT_WS2812_POWER_PIN
#else
    // default to pin 1 if the board doesn't have a default WS2812 power pin defined
    #define WS2821_POWER_PIN 1
#endif

#define WS2812_COLOR_PADDLE ((uint32_t) (255) << 8) | ((uint32_t) (255) << 16) | (uint32_t) (255)           // r << 8 | g << 16 | b
#define WS2812_COLOR_SERIAL ((uint32_t) (0) << 8) | ((uint32_t) (255) << 16) | (uint32_t) (0)
#define WS2812_COLOR_OFF ((uint32_t) (0) << 8) | ((uint32_t) (0) << 16) | (uint32_t) (0)

/*
 * constructor for the morse code sound generator with default frequency and speed
 * @param sample_rate: sample rate of the audio signal
 * @param sample_buffer_size: size of the buffer used to transmit the audio signal
 */
CWGenerator::CWGenerator(uint32_t sample_rate, uint32_t sample_buffer_size) : CWGenerator(sample_rate, sample_buffer_size, DEFAULT_FREQUENCY, DEFAULT_WPM, DEFAULT_VOLUME) {}

/*
 * constructor for the morse code sound generator
 * @param sample_rate: sample rate of the audio signal
 * @param sample_buffer_size: size of the buffer used to transmit the audio signal
 * @param freq: frequency of the audio signal
 * @param wpm: speed of the morse code in WPM (Words Per Minute)
 * @param volume: volume of the signal [0:100]
 */
CWGenerator::CWGenerator(uint32_t sample_rate, uint32_t sample_buffer_size, uint16_t freq, uint16_t wpm, uint16_t volume) {
    curstate = STATE_INIT;
    cw_sample_rate = sample_rate;
    cw_sample_buffer_size = sample_buffer_size;
    cw_frequency = freq;
    cw_wpm = wpm;
    cw_volume = volume * 32767 / 100;

    signal_buffer = NULL;
    signal_buffer_size = 0;

    init_buffers();

    // initialize GPIO for paddle
    gpio_init(DIT_GPIO);
    gpio_init(DAH_GPIO);
    gpio_set_dir(DIT_GPIO, false);
    gpio_set_dir(DAH_GPIO, false);
    gpio_pull_up(DIT_GPIO);
    gpio_pull_up(DAH_GPIO);
    debouncer.debounce_gpio(DIT_GPIO);
    debouncer.debounce_gpio(DAH_GPIO);

    // initialize PIO used for Neopixel LED
    ws2812_pio = pio1;              // use PIO1 as default (PIO0 is used for button debouncer)
    ws2812_sm = pio_claim_unused_sm(ws2812_pio, true);
    uint offset = pio_add_program(ws2812_pio, &ws2812_program);
    gpio_init(WS2812_POWER_PIN);
    gpio_set_dir(WS2812_POWER_PIN, true);
    gpio_put(WS2812_POWER_PIN, true);                                                       // enable Neopixel LED

    ws2812_program_init(ws2812_pio, ws2812_sm, offset, WS2812_PIN, 800000, IS_RGBW);
    put_pixel(WS2812_COLOR_OFF);

    queue_init(&cw_character_queue, sizeof(CW_CHARACTERS), queue_max_char);
}

/*
 * initializes the audio buffers for the currently set frequency
 */
void CWGenerator::init_buffers() {
    if (signal_buffer != NULL) {
        free(signal_buffer);
    }

    // limit the user passed audio frequency to the valid range
    cw_frequency = cw_frequency > audio_maxfreq ? audio_maxfreq : cw_frequency;
    cw_frequency = cw_frequency < audio_minfreq ? audio_minfreq : cw_frequency;

    // calculate the audio buffer size, the start of the pause buffer and allocate the memory
    signal_buffer_period = ceil(cw_sample_rate / (float)(cw_frequency));
    signal_buffer_pause_index = ceil((double)(signal_buffer_period + cw_sample_buffer_size) / signal_buffer_period) * signal_buffer_period;
    signal_buffer_size = signal_buffer_pause_index + cw_sample_buffer_size;

    signal_buffer = (int16_t *)malloc(sizeof(int16_t) * signal_buffer_size);
    memset(signal_buffer, 0, sizeof(int16_t) * signal_buffer_size);                                             // initialize buffer with silence

    for (int i = 0; i < signal_buffer_pause_index; i++) {                                                       // generate sinus wave. Remaining part is the pause buffer
        signal_buffer[i] = cw_volume * sin(i * 2.0 * M_PI * (float)(cw_frequency) / (float)(cw_sample_rate));
    }

    signal_dit_length_index = (60 / (50 * (float)(cw_wpm))) * cw_sample_rate;                                  // length of DIT t_dit = 60 / (50 * wpm). Source: https://morsecode.world/international/timing.html
    signal_dit_length_index = ceil((float)(signal_dit_length_index) / signal_buffer_period) * signal_buffer_period;  // must be a whole multiple of the tone period to ensure tone ends after a full period
    inchar_index = 0;
}

/*
 * clears the character queue
 */
void CWGenerator::clear_queue() {
    while (queue_try_remove(&cw_character_queue, &(curchar))) {}
}

/*
 * set the integrated Neopixel to the specified color
 * @param pixel_grb: color of the Neopixel LED (r << 8 | g << 16 | b)
 */
inline void CWGenerator::put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(ws2812_pio, ws2812_sm, pixel_grb << 8u);
}

/*
 * set the audio frequency in Hz of the sine wave
 * @param freq: frequency of the audio signal.
 *              the value must be between [audio_minfreq, audio_maxfreq]
 */
void CWGenerator::set_frequency(uint16_t freq) {
    cw_frequency = freq;
    init_buffers();
}

/*
 * set the audio frequency in Hz of the sine wave
 * @return frequency of the audio signal.
 */
uint16_t CWGenerator::get_frequency() {
    return (cw_frequency);
}

/*
 * set the speed auf the morse signal in WPM (Words Per Minute)
 * @param wpm: the speed in WPM
 */
void CWGenerator::set_wpm(uint16_t wpm) {
    cw_wpm = wpm;
    init_buffers();
}

/*
 * get the speed auf the morse signal in WPM (Words Per Minute)
 * @return the speed in WPM
 */
uint16_t CWGenerator::get_wpm() {
    return (cw_wpm);
}

/*
 * set the volume of the morse signal [0:100]
 * @param volume: volume [%] of the morse signal
 */
void CWGenerator::set_volume(uint16_t vol) {
    if (vol * 32767 / 100 != cw_volume) {
        cw_volume = vol * 32767 / 100;

        if (vol > 0) {
            init_buffers();
        }
    }
}

/*
 * get the volume of the morse signal
 * @return volume of the morse signal
 */
uint16_t CWGenerator::get_volume() {
    return (cw_volume);
}

/*
 * adds a morse code character to the transmission queue
 * @param ch: character to be send out
 */
void CWGenerator::send_character(CW_CHARACTERS ch) {
    queue_add_blocking(&cw_character_queue, &ch);
}

/*
 * adds morse code characters to the transmission queue
 * @param ch: string containing characters to be send out (' ' -> Pause, '.' -> DIT, '-' -> DAH)
 */
void CWGenerator::send_character(char *ch) {
    CW_CHARACTERS cwchar;
    CW_CHARACTERS cwchar_pause = CHAR_PAUSE;

    for (int i = 0; i < strnlen(ch, 10); i++) {             // allow up to a maximum of 10 morse code characters
        if (ch[i] == '.') {
            cwchar = CHAR_DIT;
        } else if (ch[i] == '-') {
            cwchar = CHAR_DAH;
        } else {
            cwchar = CHAR_PAUSE;
        }

        queue_add_blocking(&cw_character_queue, &cwchar);
    }

    // add pause between characters (-1 because one pause unit is included with the character)
    for (int i = 0; i < INTER_CHAR_PAUSE_UNITS - 1; i++ ) {
        queue_add_blocking(&cw_character_queue, &cwchar_pause);
    }
}

/*
 * helper function to set a new state of the CW state machine
 * @param ws2812_color: color of the Neopixel LED
 * @param ch: character to be send out
 */
void CWGenerator::set_state(CW_CHARACTERS ch, uint32_t ws2812_color) {
    put_pixel(ws2812_color);

    switch (ch) {
        case CHAR_PAUSE:
            nextstate = STATE_IDLE;
            inchar_endindex = signal_dit_length_index * INTRA_CHAR_PAUSE_UNITS;
            if (curstate == STATE_DIT) {
                curstate = STATE_DIT_PAUSE;
            } else {
                curstate = STATE_DAH_PAUSE;
            }
            break;
        case CHAR_DIT:
            inchar_endindex = signal_dit_length_index * DIT_UNITS;
            curstate = STATE_DIT;
            break;
        case CHAR_DAH:
            inchar_endindex = signal_dit_length_index * DAH_UNITS;
            curstate = STATE_DAH;
            break;
        default:
            printf("ERROR: illegal character\n");
    }
}

/*
 * Updates the state machine and checks the paddle position
 */
void CWGenerator::update_statemachine() {
    if (curstate == STATE_INIT) {
        inchar_index = 0;
        inchar_endindex = cw_sample_rate / signal_buffer_period;  // wait for 1s to avoid start is not recorded
        curstate = STATE_INIT_PAUSE;
        printf("STATE_INIT_PAUSE\n");
    } else if (curstate == STATE_IDLE) {
        inchar_index = 0;

        if (nextstate == STATE_DIT) {
            clear_queue();
            set_state(CHAR_DIT, WS2812_COLOR_PADDLE);
        } else if (nextstate == STATE_DAH) {
            clear_queue();
            set_state(CHAR_DAH, WS2812_COLOR_PADDLE);
        } else {
            if (debouncer.read(DIT_GPIO) == 0) {
                clear_queue();
                set_state(CHAR_DIT, WS2812_COLOR_PADDLE);
            } else if (debouncer.read(DAH_GPIO) == 0) {
                clear_queue();
                set_state(CHAR_DAH, WS2812_COLOR_PADDLE);
            } else if (queue_try_remove(&cw_character_queue, &(curchar)) == true) {
                set_state(curchar, WS2812_COLOR_SERIAL);
            } else {
                put_pixel(WS2812_COLOR_OFF);
            }
        } 
        nextstate = STATE_IDLE;
    } else if (inchar_index > inchar_endindex) {
        inchar_index = 0;

        switch (curstate) {
            case STATE_DIT:
                set_state(CHAR_PAUSE, WS2812_COLOR_OFF);
                break;
            case STATE_DAH:
                set_state(CHAR_PAUSE, WS2812_COLOR_OFF);
                break;
            case STATE_DIT_PAUSE:
                if (debouncer.read(DAH_GPIO) == 0) {
                    set_state(CHAR_DAH, WS2812_COLOR_PADDLE);
                } else {
                    curstate = STATE_IDLE;
                    // printf("STATE_IDLE\n");
                }
                break;
            case STATE_DAH_PAUSE:
                if (debouncer.read(DIT_GPIO) == 0) {
                    set_state(CHAR_DIT, WS2812_COLOR_PADDLE);
                } else {
                    curstate = STATE_IDLE;
                    // printf("STATE_IDLE\n");
                }
                break;
            case STATE_INIT_PAUSE:
                curstate = STATE_IDLE;
                break;
            default:
                // shouldn't happen
                printf("Illegal state.\n");
                curstate = STATE_IDLE;
        }
    } else if (curstate == STATE_DIT_PAUSE) {
        // check alread during the pause for the status of the paddle to avoid missed key presses
        if (debouncer.read(DAH_GPIO) == 0) {
            nextstate = STATE_DAH;
        }
    } else if (curstate == STATE_DAH_PAUSE) {
        // check alread during the pause for the status of the paddle to avoid missed key presses
        if (debouncer.read(DIT_GPIO) == 0) {
            nextstate = STATE_DIT;
        }
    }

    inchar_index += cw_sample_buffer_size;
}

/*
 * Returns the audio buffer for the next transmission
 * @return buffer consisting of an array of int16_t samples
 */
void *CWGenerator::get_audio_buffer() {
    uint32_t pos;

    if ((curstate == STATE_DIT || curstate == STATE_DAH) && (cw_volume > 0)) {
        if (inchar_index > inchar_endindex) {
            // if this is the last part of the signal, use the last signal_buffer
            // substract one cw_sample_buffer_size as inchar is increased during update_state
            pos = ((inchar_index - cw_sample_buffer_size) % signal_buffer_period) + signal_buffer_pause_index - signal_buffer_period;
            return (&(signal_buffer[pos]));
        } else {
            pos = (inchar_index - cw_sample_buffer_size) % signal_buffer_period;
            return (&(signal_buffer[pos]));
        }
    } else {
        return (&(signal_buffer[signal_buffer_pause_index]));
    }
}

/*
 * Returns the audio buffer size for the next transmission
 * @return buffer size in uint32_t
 */
uint32_t CWGenerator::get_audio_buffer_size() {
    return (sizeof(int16_t) * cw_sample_buffer_size);
}