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

#include "winkeyer_parser.h"
#include "pico/bootrom.h"

/* 
 * class that parses the WinKeyer commands passed through a serial interface
 * Simplified parser without buffered command support. Information after command is ignored.
 */

/*
 * Mapping of ASCII characters to morse code
 * Based on the WinKeyer3 Datasheet from K1EL
 * https://www.hamcrafters2.com/WK3IC.html
 */
const char *WK123_CW_MAPPING[] = {
    "       ",  // 0x20: SPC -> PAUSE (pause between words)
    "",         // 0x21: ! -> ignored
    ".-..-.",   // 0x22: " -> RR
    "",         // 0x23: # -> ignored
    "...-..-",  // 0x24: $ -> SX
    "",         // 0x25: % -> ignored
    "",         // 0x26: & -> ignored
    ".----.",   // 0x27: ' -> WG
    "-.--.",    // 0x28: ( -> KN
    "-.--.-",   // 0x29: ) -> KK
    "",         // 0x2A: * -> ignored
    ".-.-.",    // 0x2B: + -> AR
    "--..--",   // 0x2C: ,
    "-....-",   // 0x2D: -
    ".-.-.-",   // 0x2E: .
    ".--.-",    // 0x2F: /
    "-----",    // 0x30: 0
    ".----",    // 0x31: 1
    "..---",    // 0x32: 2
    "...--",    // 0x33: 3
    "....-",    // 0x34: 4
    ".....",    // 0x35: 5
    "-....",    // 0x36: 6
    "--...",    // 0x37: 7
    "---..",    // 0x38: 8
    "----.",    // 0x39: 9
    "-.--.",    // 0x3A: : -> KN
    ".-.-",     // 0x3B: ; -> AA
    ".-.-.",    // 0x3C: < -> AR
    "-...-",    // 0x3D: = -> BT
    "...-.-",   // 0x3E: > -> SK
    "..--..",   // 0x3F: ?
    ".--.-.",   // 0x40: @ -> AC
    ".-",       // 0x41: A
    "-...",     // 0x42: B
    "-.-.",     // 0x43: C
    "-..",      // 0x44: D
    ".",        // 0x45: E
    "..-.",     // 0x46: F
    "--.",      // 0x47: G
    "....",     // 0x48: H
    "..",       // 0x49: I
    ".---",     // 0x4A: J
    "-.-",      // 0x4B: K
    ".-..",     // 0x4C: L
    "--",       // 0x4D: M
    "-.",       // 0x4E: N
    "---",      // 0x4F: O
    ".--.",     // 0x50: P
    "--.-",     // 0x51: Q
    ".-.",      // 0x52: R
    "...",      // 0x53: S
    "-",        // 0x54: T
    "..-",      // 0x55: U
    "...-",     // 0x56: V
    ".--",      // 0x57: W
    "-..-",     // 0x58: X
    "-.--",     // 0x59: Y
    "..--",     // 0x5A: Z
    ".-...",    // 0x5B: [ -> AS
    "-..-.",    // 0x5C: \ -> DN
    "-.--."     // 0x5D: ] -> KN
};

/*
 * Frequency table for WK1 / WK2 mode of operation
 */
const uint16_t WK12_FREQUENCY_LIST[] = {
    0,
    4000,
    2000,
    1333,
    1000,
    800,
    666,
    571,
    500,
    444,
    400
};

/*
 * Contains the size of each admin command including the leading 0x00
 * TODO: Parse also parital admin commands
 */
const uint8_t WK123_ADMIN_COMMAND_SIZE[] = {
    3,              // 0: Calibrate - ignored
    2,              // 1: Reset - ignored
    2,              // 2: Host Open
    2,              // 3: Host Close - ignored
    3,              // 4: Echo Test
    2,              // 5: Paddle A2D
    2,              // 6: Speed A2D
    2,              // 7: Get Values
    2,              // 8: Reserved - ignored
    2,              // 9: Get FW Major Rev
    2,              // 10: Set WK1 Mode
    2,              // 11: Set WK2 Mode
    2,              // 12: Dump EEPROM - ignored
    2,              // 13: Load EEPROM - ignored
    3,              // 14: Send Message - ignored
    3,              // 15: Load X1MODE - ignored
    2,              // 16: Firmware Update - ignored
    2,              // 17: Set Low Baud - ignored
    2,              // 18: Set High Baud - ignored
    4,              // 19: Set RTTY Mode Registers - ignored
    2,              // 20: Set WK3 Mode
    2,              // 21: Read Back Vcc
    3,              // 22: Load X2MODE - ignored
    2,              // 23: Get FW Minor Rev
    2,              // 24: Get IC Type
    3               // 25: Get Sidetone Volume
};

/* 
 * constructor for the morse code sound generator with default frequency and speed
 * @param cwgen: CWGenerator used to send text messages
 */
WinKeyerParser::WinKeyerParser(CWGenerator *cwgen) {
    cw_generator = cwgen;
}

/*
 * parses admin commands
 * @param message message to parse. message[0] corresponds to <0> which indicates an admin command
 * @param offset start position of admin message
 * @param length length of the provided message
 * @param maxsize buffer size
 * @return number of bytes added to the result;
 */
uint32_t WinKeyerParser::parse_admin_command(uint8_t *message, int *offset, uint32_t length, uint32_t maxsize) {
    int offs = *offset;
    (*offset)++;              // skip parameter in message

    // only accept full commands
    if (length - offs < 2) {
        return 0;
    }

    switch(message[offs + 1]) {
        case 0:                 // 0x00: Calibrate - ignored
            break;
        case 1:                 // 0x01: Reset - ignored
            break;
        case 2:                 // 0x02: Host Open
            message[0] = 31;    // echo back revision 31 for rev 31.03 (version according to datasheet)
            message[1] = 03;
            wk_version = 1;     // according to datasheet WK1 mode is set on host open
            return 2;
        case 3:                 // 0x03: Host Close - ignored
            break;
        case 4:                 // 0x04: Echo Test
            if (length - offs >= 3) {
                message[0] = message[offs + 2];
                (*offset)++;    // skip parameter in message
                return 1;
            }
            break;
        case 5:                 // 0x05: Paddle A2D - always return 0, according to datasheet
            message[0] = 0;
            return 1;
        case 6:                 // 0x06: Speed A2D - always return 0, according to datasheet
            message[0] = 0;
            return 1;
        case 7:                 // 0x07: Get Values - always return 0, according to datasheet
            message[0] = 0;
            return 1;
        case 8:                 // 0x08: Reserved - ignored
            break;
        case 9:                 // 0x09: Get FW Major Rev
            message[0] = 31;    // echo back revision 31 for rev 31.03 (version according to datasheet)
            return 1;
        case 10:                // 0x0A: Set WK1 Mode
            wk_version = 1;
            break;
        case 11:                // 0x0B: Set WK2 Mode
            wk_version = 2;
            break;
        case 12:                // 0x0C: Dump EEPROM - ignored
            break;
        case 13:                // 0x0D: Load EEPROM - ignored
            break;
        case 14:                // 0x0E: Send Message - ignored
            break;
        case 15:                // 0x0F: Load X1MODE - ignored
            break;
        case 16:                // 0x10: Firmware Update - ignored
            break;
        case 17:                // 0x11: Set Low Baud - ignored
            break;
        case 18:                // 0x12: Set High Baud - ignored
            break;
        case 19:                // 0x13: Set RTTY Mode Registers - ignored
            break;
        case 20:                // 0x14: Set WK3 Mode
            wk_version = 3;
            break;
        case 21:                // 0x15: Read Back Vcc
            message[0] = 52;    // always report back ~5V (according to datasheet: 26214/byte value = Voltage * 100)
            return 1;
        case 22:                // 0x16: Load X2MODE - ignored
            break;
        case 23:                // 0x17: Get FW Minor Rev
            message[0] = 03;    // echo back revision 31 for rev 31.03 (version according to datasheet)
            return 1;
        case 24:                // 0x18: Get IC Type
            message[0] = 0x01;  // always report SMT IC
            return 1;
        case 25:                // 0x19: Set Sidetone Volume - ignored as changes lead to disturbed audio on Windows
            /*(*offset)++;              // skip parameter in message
            if ((length - offs >= 3) && (message[offs + 2] >= 0) && (message[offs + 2] <= 4)) {
                cw_generator->set_volume(message[offs + 2] * 100 / 4);
            }*/
            break;
        case 26:                // 0x1A: Set rise time of Blackman window
            (*offset)++;              // skip parameter in message
            if ((length - offs >= 3) && (message[offs + 2] >= 1) && (message[offs + 2] <= 50)) {
                cw_generator->set_risetime((float)((uint8_t)message[offs + 2]));
            }
            break;
/*        case 27:                // 0x1B: Get rise time of Blackman window
            message[0] = (uint8_t)cw_generator->get_risetime();
            return 1;*/
        case 27:                // 0x1B: Set 
            (*offset)++;
            cw_generator->set_frequency((uint8_t)message[offs + 2] * 10);
            break;
        case 28:                // 0x1C: enter bootloader with default values
            reset_usb_boot(0, 0);
        default:                // Unknown admin command - ignore
            break;
    }

    return 0;
}

/*
 * parses the provided message and acts accordingly
 * @param message: byte array containing the message received through a serial interface
 * @param length: size of the message
 * @return number of bytes added to the message buffer
 */
uint32_t WinKeyerParser::parse_message(uint8_t *message, uint32_t length, uint32_t maxsize) {
    if ((length == 0) || (message == NULL)) {
        return 0;
    }

    for (int i = 0; i < length; i++) {
        if ((message[i] >= 0x61) && (message[i] <= 0x7a)) {
            // convert small letters to upper case
            message[i] -= 0x20;
        }
        // check if the message starts with CW text
        if (message[i] >= cw_mapping_min_ascii && message[i] <= cw_mapping_max_ascii) {
            cw_generator->send_character((char *)WK123_CW_MAPPING[message[i] - cw_mapping_min_ascii]);
        } else {
            // check for commands

            switch (message[i]) {
                case 0x00:                // Admin command
                    return parse_admin_command(message, &i, length, maxsize);
                case 0x01:                // Sidetone Freq
                    if (length >= 2) {
                        if ((wk_version < 3) && (message[i+1] >= 1) && (message[i+1] <= 0x0a)) {
                            cw_generator->set_frequency(WK12_FREQUENCY_LIST[message[i+1]]);
                        } else if ((wk_version == 3) && (message[i+1] >= 15) && (message[i+1] <= 125)) {
                            cw_generator->set_frequency(62500/message[i+1]);
                        }
                        i++;              // skip parameter in message
                    }
                    break;
                case 0x02:                // Speed
                    if ((length >= 2) && (message[i+1] >= 5) && (message[i+1] <= 99)) {
                        cw_generator->set_wpm(message[i+1]);
                        i++;              // skip parameter in message
                    }
                    break;
                case 0x03:                // Weighting - ignored
                    break;
                case 0x04:                // PTT Lead-in/Tail - ignored
                    break;
                case 0x05:                // Speed Pot Setup - ignored
                    break;
                case 0x06:                // Pause - ignored
                    break;
                case 0x07:                // Get Speed Pot
                    message[0] = (cw_generator->get_wpm() & 0x3F) | 0x80;
                    return 1;
                    break;
                case 0x08:                // Backspace - ignored
                    break;
                case 0x09:                // Pin Configuration - ignored
                    break;
                case 0x0A:                // Clear Buffer - ignored
                    break;
                case 0x0B:                // Key Immediate - ignored
                    break;
                case 0x0C:                // HSCW Speed - ignored
                    break;
                case 0x0D:                // Farnsworth - ignored
                    break;
                case 0x0E:                // WinKeyer3 Mode
                    wk_version = 3;
                    break;
                case 0x0F:                // Load Defaults - ignored
                    break;
                case 0x10:                // First Extension - ignored
                    break;
                case 0x11:                // Key Compensation - ignored
                    break;
                case 0x12:                // Paddle Switchpoint - ignored
                    break;
                case 0x13:                // ignored
                    break;
                case 0x14:                // S/W Paddle Input - ignored
                    break;
                case 0x15:                // WinKeyer3 Status
                    message[0] = 0xC0;    // always return default status
                    return 1;
                case 0x16:                // Buffer Pointer - ignored
                    break;
                case 0x17:                // Dit/Dah Ratio - ignored
                    break;
                case 0x18:                // PTT Control - ignored
                    break;
                case 0x19:                // Key Bufferd - ignored
                    break;
                case 0x1A:                // Wait - ignored
                    break;
                case 0x1B:                // Merge Letters - ignored
                    break;
                case 0x1C:                // Speed Change - ignored
                    break;
                case 0x1D:                // HSCW Speed - ignored
                    break;
                case 0x1E:                // Cancel Buff Speed - ignored
                    break;
                case 0x1F:                // Buffered NOP - ignored
                    break;
                default:                  // unknown command, ignore
                    break;
            }
        }
    }
    return 0;
}