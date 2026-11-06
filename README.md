# PistonPuller

[English](README.md) | [简体中文](README_zh.md)

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-orange.svg)](#)
[![Version](https://img.shields.io/badge/Version-0.3.0-brightgreen.svg)](#)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**PistonPuller** is a minimalist, hardcore CLI downloader for Minecraft Java Edition (Vanilla). 

It pulls core files, libraries, and assets directly from the Mojang Piston Meta API. No bloated GUIs, no unnecessary dependencies, just pure C code doing exactly what it's supposed to do.

## Why PistonPuller?

Recently, many mainstream third-party launchers have restricted their vanilla game download features due to compliance and regulatory pressures. While some users can bypass this with proxies, the core issue remains: relying on a monolithic launcher for a simple download is inefficient.

We built PistonPuller because we believe in the Unix philosophy: **Do one thing and do it well.** 

Furthermore, PistonPuller is designed as a **"Geek Filter."** The CLI interface is not an accident—it is a deliberate threshold. We believe that true players who love the game and the community possess the patience to read a README, open a terminal, and understand basic file structures. If you are looking for a one-click "baby-feeding" GUI, this tool is not for you.

## Features

- 🚀 **Blazing Fast & Lightweight**: Written in C. No Node.js, no Python, no Java runtime required.
- ⚡ **16-Thread Acceleration**: Utilizes a multithreading thread pool. Downloads thousands of assets and libraries concurrently, boosting speed by over 10x.
- ⏯️ **Resume Support (Breakpoint Resume)**: Automatically detects partially downloaded files and resumes from where it left off. Never download from scratch again.
- 📊 **apt-style Progress Bar**: A gorgeous terminal progress bar with real-time speed (MB/s) and ETA (Estimated Time of Arrival), keeping your console clean.
- 🌍 **Cross-Platform**: Native support for Linux and Windows. Windows builds are completely standalone `.exe` files.
- 🧠 **Smart Sync & Auto-Retry**: Automatically checks local file sizes to skip complete files. Includes a 3-time auto-retry mechanism for unstable networks.
- 🔍 **Dynamic Versioning**: Supports fuzzy queries like `latest`, `snapshot`, and full version listings.
- 🛡️ **Asset Control**: Use the `--no-assets` flag to skip downloading thousands of tiny resource files.
- 📦 **Launcher Compatible**: Files are downloaded to standard directory structures, fully recognizable by HMCL, MultiMC, PrismLauncher, etc.

## Prerequisites & Build

### 🐧 Linux
Install dependencies (Debian/Ubuntu):
```bash
sudo apt update
sudo apt install libcurl4-openssl-dev
```

Clone and compile:
```bash
git clone https://github.com/Douglas-Woods2023/PistonPuller.git
cd PistonPuller
gcc main.c cJSON.c -o pistonpuller -lm -lcurl
```

### 🪟 Windows (MSYS2)
It is highly recommended to use [MSYS2](https://www.msys2.org/) to compile this project on Windows. 

Open the MSYS2 MINGW64 terminal and install the toolchain and dependencies:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl
```

Clone and compile (Dynamic linking):
```bash
git clone https://github.com/Douglas-Woods2023/PistonPuller.git
cd PistonPuller
gcc main.c cJSON.c -o pistonpuller.exe -lm -lcurl -lpthread
```

**How to run on other PCs:**
Because `libcurl` on Windows relies on dynamic libraries, you need to gather the required `.dll` files to distribute the `.exe`.
You can use `ldd pistonpuller.exe` in MSYS2 to see the dependencies, and copy them (like `libcurl-x64.dll`, `libcrypto-3-x64.dll`) into the same directory as `pistonpuller.exe`. Then zip them together for distribution.

## Usage

### Linux
```bash
./pistonpuller [version] [install_dir] [--no-assets]
```

### Windows (CMD / PowerShell)
```cmd
pistonpuller.exe [version] [install_dir] [--no-assets]
```

### Examples

**1. Download the latest release version to the default directory:**
```bash
# Linux
./pistonpuller latest
# Windows
pistonpuller.exe latest
```

**2. Download a specific version (e.g., 26.3) and skip assets:**
```bash
# Linux
./pistonpuller 26.3 --no-assets
# Windows
pistonpuller.exe 26.3 --no-assets
```

**3. List all available game versions (Piping is your friend):**
```bash
# Linux (using grep)
./pistonpuller list | grep release | head -n 10

# Windows PowerShell (using Select-String)
.\pistonpuller.exe list | Select-String "release" | Select-Object -First 10
```

## How It Works

1. Fetches `version_manifest_v2.json` from Piston Meta API.
2. Parses the JSON using `cJSON` to resolve the target version's metadata URL.
3. Downloads `client.jar` and saves the version `.json`.
4. Iterates through `libraries` and downloads them into `.minecraft/libraries/`.
5. Parses `assetIndex` and batch-downloads resources into `.minecraft/assets/objects/`.

## Disclaimer

This tool is for **educational and auxiliary purposes only**. It assists users in downloading game files directly from Mojang's official servers. 

- It **does not** host, distribute, or modify any copyrighted game assets.
- Minecraft is a trademark of Mojang Studios.
- Users are solely responsible for complying with local laws and Mojang's EULA. The author assumes no liability for any misuse.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---
*Built with ❤️ and ☕ by a student developer for the hardcore MC community.*