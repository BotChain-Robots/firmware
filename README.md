# Firmware

1. [Setup](#Setup)
    1. [Install Dependencies](#InstallDependencies)
        1. [MacOS](#MacOS1)
        2. [Windows](#Windows1)
2. [Development](#Development)
    1. [Command Line Tools](#CommandLineTools)
        1. [MacOS](#MacOS2)
        2. [Windows](#Windows2)
    2. [Using an IDE](#UsinganIDE)
        1. [Visual Studio Code](#VisualStudioCode)
        2. [JetBrains CLion](#JetBrainsCLion)

<hr>

## Setup <a name="Setup"></a>
**This project is currently setup to work with the ESP32-S3 board.** 
We use ESP-IDF version 5.5.1. 
The Espressif docs have a [well written article](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html) that explains the build system.
Versions before 5.5 are incompatible due to an issue with the CMakeLists.txt file in the ledc component (that we depend on).

### Install Dependencies <a name="InstallDependencies"></a>
#### MacOS <a name="MacOS1"></a>
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
./install.sh esp32,esp32s3
```

#### Windows <a name="Windows1"></a>
Install ESP-IDF with with the GUI [Windows installer](https://dl.espressif.com/dl/esp-idf/?idf=4.4). 
- Use default options.
- You may install ESP-IDF to a path that is different than the default, however, keep the length of the path under 90 characters and do not include any spaces or non ASCII characters. 
- Select version v5.5.x.
- Ensure that the `esp32s3` checkbox is selected (should be by default)
- The installer will automatically provide
    - Embedded Python
    - Required cross compilers
    - OpenOCD
    - CMake and Ninja
    - ESP-IDF


## Development <a name="Development"></a>
The board is flashed via UART. A USB to UART adapter is provided as part of the devboard.

### Command Line Tools <a name="CommandLineTools"></a>
#### MacOS <a name="MacOS2"></a>
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

#### Windows <a name="Windows2"></a>
Run the following commands in a command prompt.

Export environment variables (alternatively run the ESP-IDF Command Prompt shortcut)
```
# Example path: C:\Espressif\frameworks\esp-idf-v5.5.1-3\

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

### Using an IDE <a name="UsinganIDE"></a>
Any IDE that supports CMake or has an ESP-IDF extension should be compatible with this project.

#### Visual Studio Code <a name="VisualStudioCode"></a>
ESP-IDF has an official extension for Visual Studio Code. Install the CMake Tools, Dev Containers, WSL, and ESP-IDF extensions.

Use the buttons at the bottom of the IDE to build, flash & monitor the board. You may need to change the COM port (at the bottom of the IDE).

#### JetBrains CLion <a name="JetBrainsCLion"></a>
CLion is compatible with this project. Follow [the tutorial on Espressif's site](https://developer.espressif.com/blog/clion/) with the following modifications:
- When asked to select a project, simply import the root directory of this repository (ie. the `firmware` folder).
- If the `Open Project Wizard` does not open, access it via `Settings > Build, Execution, Deployment > CMake`.
- When setting the `DIDF_TARGET`, ensure you specify `esp32s3`.
