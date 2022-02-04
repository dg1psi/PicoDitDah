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


#ifndef _WINKEYER_PARSER_H_
#define _WINKEYER_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include "cw_generator.h"

/* 
 * class that parses the WinKeyer commands passed through a serial interface
 */
class WinKeyerParser {

public:
    const static uint8_t cw_mapping_min_ascii = 0x20;  // minimum ascii character interpreted as CW text
    const static uint8_t cw_mapping_max_ascii = 0x5D;  // maximum ascii character interpreted as CW text

    /* 
     * constructor for the morse code sound generator with default frequency and speed
     * @param cwgen: CWGenerator used to send text messages
     */
    WinKeyerParser(CWGenerator *cwgen);

    /*
     * parses the provided message and acts accordingly
     * @param message: byte array containing the message received through a serial interface
     * @param length: size of the message
     * @param maxsize: maximum size of the message buffer
     * @return number of bytes added to the message buffer
     */
    uint32_t parse_message(uint8_t *message, uint32_t length, uint32_t maxsize);

private:
    CWGenerator *cw_generator;          // CWGenerator used to send text messages
    uint8_t wk_version = 3;             // current WinKeyer version

    /*
     * parses admin commands
     * @param message message to parse. message[0] corresponds to <0> which indicates an admin command
     * @param offset start position of admin message
     * @param length length of the provided message
     * @param maxsize buffer size
     * @return number of bytes added to the result;
     */
    uint32_t parse_admin_command(uint8_t *message, int *offset, uint32_t length, uint32_t maxsize);
};

#endif