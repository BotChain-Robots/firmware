# firmware

**This project is currently setup to work with the ESP32, we will be using the ESP32-c6 in the future.**

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
. ~/esp/esp-idf/export.sh
```

Build the project
```
idf.py build
```

Flash and open serial monitor
```
idf.py flash monitor
```

## Using an IDE
Any IDE that supports CMake should be compatible with this project.
### JetBrains CLion
CLion is compatible with this project. Follow [the tutorial on Espressif's site](https://developer.espressif.com/blog/clion/) with the following modifications:
- When asked to select a project, simply import the root directory of this repository (ie. the `firmware` folder).
- If the `Open Project Wizard` does not open, access it via `Settings > Build, Execution, Deployment > CMake`.
- When setting the `DIDF_TARGET`, do not use `esp32s3`, instead, use `-DIDF_TARGET=esp32`.
