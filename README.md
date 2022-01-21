# PicoDitDah
Morse code keyer based on the Raspberry Pi Pico. The device acts as a virtual USB microphone which sends pure sine waves based on the connected paddle.

The USB code uses the [TinyUSB library](https://github.com/hathach/tinyusb) to act as a virtual USB sound card. The morse code is generated purely in software on the Raspberry Pi Pico (rp2040).

The code for the microphone is based on the [audio-test example](https://github.com/hathach/tinyusb/tree/4bfab30c02279a0530e1a56f4a7c539f2d35a293/examples/device/audio_test) from the TinyUSB library and the [USB Microphone example](https://github.com/ArmDeveloperEcosystem/microphone-library-for-pico/tree/main/examples/usb_microphone) by ArmDeveloperEcoSystem.

To debounce the paddle keys the [PIO Button Debouncer by GitJer](https://github.com/GitJer/Some_RPI-Pico_stuff/tree/main/Button-debouncer) is used.

The code to drive the NeoPixel LED on the QT Py RP2040 or ItsyBitsy RP2040 is based on the [WS2812 example part of the pico-sdk examples](https://github.com/raspberrypi/pico-examples/tree/master/pio/ws2812).

![Image of the PicoDitDah build with the adafruit QT Py RP2040](docs/PicoDitDah.jpg?raw=true)

# Hardware
I use a [adafruit ItsyBitsy RP2040](https://learn.adafruit.com/adafruit-itsybitsy-rp2040) for development and a [adafruit QT Py RP2040](https://learn.adafruit.com/adafruit-qt-py-2040) for the final device.

In addition only a stereo jack to connect the morse code paddle to the Raspberry Pi Pico.

By default GPIO 3 is used for DIT and GPIO 4 for DAT. The ground pin must be connected to the ground of the Raspberry Pi Pico board.

The default GPIO configuration can be changed in the file `cw_generator.cpp`.

# Software
Once connected to the computer, the device provides two interfaces. One USB microphone and a serial port.

To change the amplitude of the morse code sine wave, use the operating system controls.

More details, like the WPM speed, can be configured using the serial port. The device uses the [WinKeyer3 protocol by K1EL](https://www.k1elsystems.com/WK3IC.html) (see the datasheet at the bottom of the page).

## Please note:
At the moment the settings are not saved. The device will always return to the default settings upon reboot.

# Case
You can find a [FreeCAD](https://www.freecadweb.org/) of the case shown above in the `case` subfolder.

This case fits for the [adafruit QT Py RP2040](https://learn.adafruit.com/adafruit-qt-py-2040).