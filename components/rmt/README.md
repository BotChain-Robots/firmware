# ESP32-S3 RMT

WIP

The related ESP32-S3 latest documentation on RMT can be found [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/rmt.html).

# About

This component is a wrapper around the ESP32-S3 RMT API and allows the operation of a physical layer between multiple ESP32-S3 microcontrollers. The goal of this component is to enable network communication encapsulation with higher and more abstract types of communication frames/packets (eg. link layer, application layer).

## Encoding Method

RMT uses the Ethernet Manchester encoding method, where a bit 1 is encoded as a falling edge transition and a bit 0 is encoded as a rising edge transition.

Specific timings are defined in `RMTSymbols.h`, which includes bit timings, resolution HZ, and symbol definitions.

## Usage

This component is not meant to be used directly by the user (should only be exclusively be used by the Link Layer, found in `components/dataLink`).

For specific details, see `dataLink/DataLinkManager.cpp`.

## Channel Configurations

RMT relies on the ESP32-S3's GPIO pins. This RMT component statically sets a predefined GPIO pin to either a TX or RX for a channel. 

For TX/RX pin definitions, see `RMTManager.h`. For example, `tx_gpio[]` is a `4` length array. Each index in the array represents one half of a pair that represents the TX/RX channel on the physical layer.

## RMT Internal Async Jobs

See the ESP32-S3 RMT documentation for more information. RMT relies on callback functions to notify the encoding/decoding on TX/RX respectively is completed, or to perform the actual encoding and decoding/char translations.