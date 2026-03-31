# RAM-Reclaimer
RAM Reclaimer - efficient Windows memory cleaner with GUI and CLI modes. Frees up RAM, cleans working sets, and optimizes system memory usage.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue)](https://www.microsoft.com/windows)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)

**RAM Reclaimer** is a Windows memory management utility that frees up RAM by trimming the working sets of processes. It operates in two modes: graphical user interface (GUI) and command-line interface (CLI).

## About

Modern operating systems do not always manage memory distribution between processes efficiently. RAM Reclaimer provides users with a tool to manually or automatically free memory that has been allocated to processes but is temporarily unused.

This project was created as an educational and practical exercise in Windows programming using WinAPI and C++20. The code is open for study, use, and modification.

## Features

- **Graphical user interface** with light, dark, and system theme support
- **Command-line utility** for scripting and automation
- Working set trimming for all processes or only the current process
- Real-time memory status display (physical RAM, pagefile, memory load)
- Support for three interface languages: English, Russian, German
- Event logging within the program interface
- UAC elevation directly from the interface
- Settings persistence (theme, language, refresh interval) in INI file

## Technologies

- **Language:** C++20
- **API:** WinAPI, Windows SDK
- **Build:** CMake 3.24+
- **Compilers:** MinGW-w64, MSVC 2022
- **Graphics:** GDI, DWM API

## Installation

### Pre-built Binaries

1. Go to the [Releases](https://github.com/dev-scoria/RAM-Reclaimer/releases) section
2. Download the latest version archive
3. Extract to any folder
4. Run `ramc_gui.exe` (GUI version) or `ramc.exe` (console version)

### Building from Source

#### Requirements

- CMake 3.24 or higher
- C++20 compatible compiler (MinGW-w64, MSVC 2022)
- Windows SDK

#### Build with MinGW-w64

```bash
git clone https://github.com/dev-scoria/RAM-Reclaimer.git
cd RAM-Reclaimer
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
make
```

#### Build with Visual Studio
```bash
git clone https://github.com/dev-scoria/RAM-Reclaimer.git
cd RAM-Reclaimer
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Compiled files will appear in the **build/Release/** folder.

## Usage

### Graphical Interface (ramc_gui.exe)

Launch `ramc_gui.exe`. The main window provides the following actions:

| Button | Description |
|--------|-------------|
| **Refresh** | Update memory status information |
| **Trim (self)** | Trim working set of RAM Reclaimer only |
| **Trim (all)** | Trim working sets of all accessible processes |
| **Settings** | Configure theme, language, auto-refresh interval |
| **Elevate** | Restart with administrator privileges (required to trim other processes) |

The **STATUS** window displays:
- Total and available physical RAM
- Total and available pagefile size
- Current memory load percentage

The **LOG** window displays program events (startup, trim results, errors).

The **LOG** window displays program events (startup, trim results, errors).

#### Command Line (ramc.exe)

```bash
ramc status                # Display current memory status
ramc clean                 # Trim working set of the current process
ramc clean --all           # Trim working sets of all accessible processes
ramc help                  # Display help
```
*Note: Running with administrator privileges is recommended for trimming processes owned by other users.*

### Configuration

Settings are stored in:
**%APPDATA%\RAM Reclaimer\settings.ini**

**Available parameters:**

   - theme — 0 (system), 1 (light), 2 (dark)

   - language — 0 (English), 1 (Russian), 2 (German)

   - refresh_seconds — auto-refresh interval for status (seconds)

### Roadmap

- Scheduled automatic cleanup

- Smart cleanup excluding critical processes

- Selective cleanup by process list

- Statistics and memory usage graphs

- File-based logging system

### Contributing

**The project is open to suggestions and improvements. To contribute:**

   - Fork the repository

   - Create a feature branch (git checkout -b feature/amazing-feature)

   - Commit your changes (git commit -m 'Add some amazing feature')

   - Push to the branch (git push origin feature/amazing-feature)

   - Open a Pull Request

### License

**This project is distributed under the MIT License. See the LICENSE file for details.**

### Contact

**Scoria Developers Portal**
**GitHub: dev-scoria**

*If you have questions, suggestions, or found a bug, please open an Issue in the repository.*
