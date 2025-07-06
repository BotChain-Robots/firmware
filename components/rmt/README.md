# ESP32-S3 RMT

WIP

The related ESP32-S3 latest documentation on RMT can be found [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/rmt.html).

## Encoding
Using the Ethernet Manchester encoding method where a bit 1 is encoded as a falling edge transition (high -> low) and a bit 0 is encoded as a rising edge transition (low -> high). 

Specific timings are defined in `RMTSymbols.h` but it has been tested with 10us intervals (each symbol has length 20us) with 1MHz resolution.

Physical Layer will be using Manchester encoding (see the doc/detailed_design_timeline document for more information).

### Testing with other Encoding Methods
We may test and compare with other encoding methods to see which has better performance. Currently, we are using manchester at 1MHz resolution - probably good enough for 10Mbps? (needs to be tested)

The following are some potential other encoding methods (inspired from http://units.folder101.com/cisco/sem1/Notes/ch7-technologies/encoding.htm)
- Manchester with shorter symbol durations (and with higher resolutions?) - Used by 100Mbps Ethernet
- NRZ Inverted - Used by 100BASE-FX networks
- 8b/10b with NRZ - Used by 1000BASE-X

## Transceiver Operations
We are currently only using one channel for TX and one channel for RX. This will be changed in the future to use multiple channels at the same time (transmitting separate data however/transmitting independent of each other).

Currently, TX is being transmitted from `RMT_TX_GPIO` and RX being received from `RMT_RX_GPIO`.

## Completed Encoding Methods
- Manchester
- NRZ-I

## Testing
Use `-D TIME_TEST=1` to measure the average transmission rate (over 1000 iterations) on a chosen encoding scheme.

To change encoding schemes, use one of the following compiler flags:
- `MANCHESTER_40=1` for 40MHz resolution Manchester
- `NRZ-INVERTED=1` for Non-Return-to-Zero Inverted
- with no specified encoding schemes, the program will use Manchester at 1MHz resolution.

### Notes
You may need to perform a `idf.py clean` or `idf.py fullclean` to undefine the unwanted compiler flags previously set (eg. when changing encoding schemes or not running the time test) 

