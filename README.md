# PicoDitDah
Morse code keyer based on the Raspberry Pi Pico. The device acts as a virtual USB microphone which sends pure sine waves based on the connected paddle.

The USB code uses the [TinyUSB library](https://github.com/hathach/tinyusb) to act as a virtual USB sound card. The morse code is generated purely in software on the Raspberry Pi Pico (rp2040).

The code for the microphone is based on the [audio-test example](https://github.com/hathach/tinyusb/tree/4bfab30c02279a0530e1a56f4a7c539f2d35a293/examples/device/audio_test) from the TinyUSB library and the [USB Microphone example](https://github.com/ArmDeveloperEcosystem/microphone-library-for-pico/tree/main/examples/usb_microphone) by ArmDeveloperEcoSystem.

# Hardware
I use a [adafruit ItsyBitsy RP2040](https://learn.adafruit.com/adafruit-itsybitsy-rp2040) for development and a [adafruit QT Py RP2040](https://learn.adafruit.com/adafruit-qt-py-2040) for the final device.

In addition only a stereo jack to connect the morse code paddle to the Raspberry Pi Pico.

By default GPIO 3 is used for DIT and GPIO 4 for DAT. The ground pin must be connected to the ground of the Raspberry Pi Pico board.

The default GPIO configuration can be changed in the file `cw_generator.cpp`.
