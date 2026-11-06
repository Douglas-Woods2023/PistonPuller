# PistonPuller

[English](README.md) | [简体中文](README_zh.md)

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-orange.svg)](#)
[![Version](https://img.shields.io/badge/Version-0.3.0-brightgreen.svg)](#)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

**PistonPuller** 是一个极简、硬核的 Minecraft Java 版（原版）命令行下载器。

它直接从 Mojang 的 Piston Meta API 拉取核心文件、依赖库和资源文件。没有臃肿的图形界面，没有多余的环境依赖，只有纯粹的 C 语言代码在做它该做的事。

## 为什么要有 PistonPuller？

近期，由于合规与监管压力，许多主流第三方启动器限制了原版游戏的下载功能。虽然部分用户可以通过代理绕过限制，但核心问题依然存在：**仅仅为了下载几个文件，去依赖一个庞大的启动器是低效的。**

我们开发 PistonPuller，是因为我们信奉 Unix 哲学：**“只做一件事，并把它做到极致。”**

此外，PistonPuller 被设计成一个 **“极客筛选器”** 。命令行界面不是我们的疏忽，而是 **故意的门槛**。我们相信，真正热爱这款游戏和社区的玩家，有足够的耐心去阅读 README、打开终端，并理解基础的目录结构。如果你在寻找一个“一键傻瓜式”的图形工具，那么这个工具并不适合你。

## 特性

- 🚀 **纯粹且轻量**：纯 C 语言编写。运行它不需要安装 Node.js、Python 或 Java 运行时。
- ⚡ **16 线程极速并发**：底层采用多线程队列模型，数千个依赖库与资源文件同时开下，速度提升 10 倍以上。
- ⏯️ **断点续传支持**：自动检测本地未下完的残卷。网络中断或手动 Ctrl+C 后，再次运行将从断点处继续，拒绝重复下载。
- 📊 **对标 `apt` 的终端进度条**：不仅有一条死死咬住底线的全局进度条，还实时显示下载速度（MB/s）与预计剩余时间（ETA），极致终端美学。
- 🌍 **全平台支持**：原生支持 Linux 与 Windows。Windows 版本解压即用，零外部依赖。
- 🧠 **智能校验与自动重试**：自动校验文件大小跳过完整文件；内置网络波动容错，失败自动重试最多 3 次，拒绝烂尾。
- 🔍 **动态版本控制**：支持 `latest`（最新正式版）、`snapshot`（最新快照版）快捷指令，以及全量版本列表查询。
- 🛡️ **资源下载开关**：使用 `--no-assets` 参数跳过数千个小体积资源文件的下载。
- 📦 **启动器兼容**：文件严格下载至标准 `.minecraft` 目录结构，HMCL、MultiMC 等启动器完美识别。

## 环境依赖与编译

### 🐧 Linux
在 Debian/Ubuntu 上安装依赖：
```bash
sudo apt update
sudo apt install libcurl4-openssl-dev
```

克隆并编译：
```bash
git clone https://github.com/Douglas-Woods2023/PistonPuller.git
cd PistonPuller
gcc main.c cJSON.c -o pistonpuller -lm -lcurl
```

### 🪟 Windows (MSYS2)
强烈推荐使用 [MSYS2](https://www.msys2.org/) 在 Windows 上进行编译。

打开 MSYS2 MINGW64 终端，安装工具链和依赖：
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl
```

克隆并编译（动态链接）：
```bash
git clone https://github.com/Douglas-Woods2023/PistonPuller.git
cd PistonPuller
gcc main.c cJSON.c -o pistonpuller.exe -lm -lcurl -lpthread
```

**如何在其他电脑上运行：**
由于 Windows 下 `libcurl` 依赖动态链接库，你需要将相关的 `.dll` 文件和 `.exe` 放在一起才能发给别人用。
在 MSYS2 终端中运行 `ldd pistonpuller.exe`，你会看到它所依赖的 DLL（如 `libcurl-x64.dll` 等），把它们全部复制到 `pistonpuller.exe` 所在的文件夹中，打包成 zip 压缩包即可完美分享。

## 使用说明

### Linux
```bash
./pistonpuller [版本号] [安装路径] [--no-assets]
```

### Windows (CMD / PowerShell)
```cmd
pistonpuller.exe [版本号] [安装路径] [--no-assets]
```

### 示例

**1. 下载最新正式版到默认目录：**
```bash
# Linux
./pistonpuller latest
# Windows
pistonpuller.exe latest
```

**2. 下载特定版本（如 26.3）并跳过资源文件下载：**
```bash
# Linux
./pistonpuller 26.3 --no-assets
# Windows
pistonpuller.exe 26.3 --no-assets
```

**3. 列出所有可用版本（管道过滤是你的好帮手）：**
```bash
# Linux (使用 grep 和 head)
./pistonpuller list | grep release | head -n 10

# Windows PowerShell (使用 Select-String)
.\pistonpuller.exe list | Select-String "release" | Select-Object -First 10
```

## 工作原理

1. 从 Piston Meta API 获取 `version_manifest_v2.json`。
2. 使用 `cJSON` 解析 JSON，获取目标版本的详细元数据 URL。
3. 下载 `client.jar` 并在本地保存版本 `.json` 文件。
4. 遍历 `libraries` 数组，将依赖库下载至 `.minecraft/libraries/`。
5. 解析 `assetIndex`，批量下载资源文件至 `.minecraft/assets/objects/`。

## 免责声明

本工具仅用于**教育与辅助目的**。它帮助用户直接从 Mojang 官方服务器下载游戏文件。

- 本工具**不托管、不分发、不修改**任何受版权保护的游戏资源。
- Minecraft 是 Mojang Studios 的商标。
- 用户需自行确保其使用行为符合当地法律法规以及 Mojang 的最终用户许可协议（EULA）。作者不对任何滥用行为承担责任。

## 开源协议

本项目采用 MIT 协议开源。详情请参阅 [LICENSE](LICENSE) 文件。

---
*由一名中学生开发者，用 ❤️ 和 ☕ 为硬核 MC 社区构建。*