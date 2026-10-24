# PistonPuller

[English](README.md) | [简体中文](README_zh.md)

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-Linux-orange.svg)](#)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**PistonPuller** is a minimalist, hardcore CLI downloader for Minecraft Java Edition (Vanilla). 

It pulls core files, libraries, and assets directly from the Mojang Piston Meta API. No bloated GUIs, no unnecessary dependencies, just pure C code doing exactly what it's supposed to do.

## Why PistonPuller?

Recently, many mainstream third-party launchers have restricted their vanilla game download features due to compliance and regulatory pressures. While some users can bypass this with proxies, the core issue remains: relying on a monolithic launcher for a simple download is inefficient.

We built PistonPuller because we believe in the Unix philosophy: **Do one thing and do it well.** 

Furthermore, PistonPuller is designed as a **"Geek Filter."** The CLI interface is not an accident—it is a deliberate threshold. We believe that true players who love the game and the community possess the patience to read a README, open a terminal, and understand basic file structures. If you are looking for a one-click "baby-feeding" GUI, this tool is not for you.

## Features

- 🚀 **Pure & Lightweight**: Written in C. No Node.js, no Python, no Java runtime required to run the downloader.
- 🧠 **Smart Sync**: Automatically checks local file sizes. Skips existing complete files and re-downloads corrupted/incomplete ones.
- 🔍 **Dynamic Versioning**: Supports fuzzy queries like `latest`, `snapshot`, and full version listings.
- 🛡️ **Asset Control**: Use the `--no-assets` flag to skip downloading thousands of tiny resource files (let launchers like HMCL or PrismLauncher fill them in later).
- 📦 **Launcher Compatible**: Files are downloaded to standard directory structures (`~/.minecraft`), fully recognizable by HMCL, MultiMC, PrismLauncher, etc.

## Prerequisites

- **OS**: GNU/Linux (Windows support planned for v0.2.0)
- **Compiler**: `gcc`
- **Libraries**: `libcurl` and `cJSON`.

Install dependencies on Debian/Ubuntu/Kali:
```bash
sudo apt update
sudo apt install libcurl4-openssl-dev
```
*(Note: `cJSON.c` and `cJSON.h` are included in this repository. No need to install them system-wide.)*

## Build

Clone the repository and compile the source code:

```bash
git clone https://github.com/Douglas-Woods2023/PistonPuller.git
cd PistonPuller
gcc main.c cJSON.c -o pistonpuller -lm -lcurl
```

## Usage

```bash
./pistonpuller [version] [install_dir] [--no-assets]
```

### Examples

**1. Download the latest release version to `~/.minecraft`:**
```bash
./pistonpuller latest
```

**2. Download a specific version (e.g., 26.3) and skip assets:**
```bash
./pistonpuller 26.3 --no-assets
```

**3. Download to a custom directory:**
```bash
./pistonpuller 1.12.2 /path/to/my/custom/minecraft
```

**4. List all available game versions (Piping is your friend):**
```bash
# List all versions
./pistonpuller list

# Find recent releases (Linux/macOS)
./pistonpuller list | grep release | head -n 10
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