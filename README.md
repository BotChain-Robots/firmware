# firmware

**This project is currently setup to work with the ESP32, we will be using the ESP32-c6 in the future.**

## Setup
This project uses ESP-IDF version 5.4.

### Install Dependencies
#### MacOS
Install xcode command line tools (if you do not already have them)
```
xcode-select --install
```

Install project & ESP-IDF dependencies (if you do not already have them)
```
brew install cmake ninja dfu-util python3
```

Setup ESP-IDF
```
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/isp-idf
./install.sh esp32
```

#### Windows
Install ESP-IDF with with the GUI [Windows installer](https://dl.espressif.com/dl/esp-idf/?idf=4.4). 
- Use default options.
- You may install ESP-IDF to a path that is different than the default, however, keep the length of the path under 90 characters and do not include any spaces or non ASCII characters. 
- Select version v5.4.x.
- The installer will automatically provide
    - Embedded Python
    - Required cross compilers
    - OpenOCD
    - CMake and Ninja
    - ESP-IDF


## Development
The board is flashed via UART. A USB to UART adapter is provided as part of the devboard.

### Command Line Tools
#### MacOS
Export environment variables
```
. ~/esp/esp-idf/export.sh
```

Build the project
```
idf.py build
```

Flash and open serial monitor (Use Ctrl+] to exit)
```
idf.py flash monitor
```

#### Windows
Run the following commands in a command prompt.

Export environment variables (alternatively run the ESP-IDF Command Prompt shortcut)
```
# Example path: C:\Espressif\frameworks\esp-idf-v5.4.1-3\

/Path/To/ESP-IDF/export.bat
```

Build the project
```
idf.py build
```

Flash and open serial monitor
```
idf.py flash monitor
```

### Using an IDE
Any IDE that supports CMake should be compatible with this project.
#### JetBrains CLion
CLion is compatible with this project. Follow [the tutorial on Espressif's site](https://developer.espressif.com/blog/clion/) with the following modifications:
- When asked to select a project, simply import the root directory of this repository (ie. the `firmware` folder).
- If the `Open Project Wizard` does not open, access it via `Settings > Build, Execution, Deployment > CMake`.
- When setting the `DIDF_TARGET`, do not use `esp32s3`, instead, use `-DIDF_TARGET=esp32`.
