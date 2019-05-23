# block-speed-test
Commandline utility for testing block measurement performance

## Building
- Install libtiepie, see https://www.tiepie.com/libtiepie-sdk
- Compile the application using the GCC compiler by running `make` (or `mingw32-make` on Windows)

## Command line options
- `-b <resolution in bits>` - Resolution for measurement. (default: highest possible)
- `-c <number of channels>` - Number of active channels for measurement. (default: all)
- `-f <sample frequency` - Sample frequency in Hz. (default: 1MHz)
- `-l <record length>` - Record length in Samples. (default: 5000)
- `-n <number of measurements>` - Number of measurement. (default: 100)
- `-s <serial number>` - Open instrument with specified serial number. (default: auto detect)
- `-r` - When specified the raw data to floating point data conversion is disabled.
- `-e` - Trigger on EXT1 instead of using no trigger.

## Example
- `./blockspeedtest -b 14 -c 1 -f 100e6 -n 1000 -r` - Performs 1000 block measurements without trigger on channel 1, measuring 5000 samples at 100 MHz @ 14 bit, passing raw data.
