# firmware

## Install Dependencies
### MacOS
Install xcode command line tools (if you do not already have them)
```
xcode-select --install
```

Install project & ESP-IDF dependencies
```
brew install cmake ninja dfu-util
brew install python3
```

Setup ESP-IDF
```
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/isp-idf
./install.sh esp32
```

## Running
The board is flashed via UART. A USB to UART adapter is provided as part of the devboard.

### MacOS
Export environment variables
```
cd ~/esp/esp-idf && . ./export.sh
```

Flashing via UART
```
idf.py -p PORT [-b BAUD] flash
```

## Connect to serial
### MacOS
Find the port
```
ls /dev/cu.*
```

View the output
```
screen /dev/cu.device_name [BAUD]
```

