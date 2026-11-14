# FastShutdown

在桌面创建「一键关机」和「一键重启」快捷方式。

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](#)
[![Platform](https://img.shields.io/badge/Platform-Windows-win.svg)](#)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

### 自动绑定 Windows 内置图标库 `SHELL32.dll`

![png](/screenshot/demo1.png)

## 功能

双击运行后，在当前用户桌面生成两个快捷方式：

| 快捷方式 | 目标 | 参数 |
| --- | --- | --- |
| 一键关机 | `%SystemRoot%\System32\shutdown.exe` | `-s -t 0` |
| 一键重启 | `%SystemRoot%\System32\shutdown.exe` | `-r -t 0` |

- 图标直接取自系统 `SHELL32.dll`（索引 27 / 238），无需附带图标文件
- 桌面路径读取注册表 `User Shell Folders`，兼容 OneDrive 桌面重定向与自定义桌面位置
- 已存在的同名快捷方式会被覆盖更新
- 单文件静态编译，无 Python 运行时、无第三方依赖

## 🛠️ 本地编译

```bat
.\build.bat
```

脚本依赖 **MinGW-w64**（`g++` 与 `windres`）。若二者不在 PATH 中，脚本会依次尝试 `C:\MinGW64\bin`、`C:\mingw64\bin`、`C:\msys64\mingw64\bin`、`C:\TDM-GCC-64\bin`；也可用环境变量显式指定：

```bat
set CXX=C:\path\to\g++.exe
set WR=C:\path\to\windres.exe
build.bat
```

产物输出到 `dist\FastShutdown.exe`；中间对象写入 `%TEMP%\FastShutdown_build`，构建结束或失败时自动清理，不污染项目目录。

手动编译：

```bat
windres -I . app.rc -o app_res.o
g++ -std=c++17 -O2 -mwindows main.cpp app_res.o -o dist\FastShutdown.exe ^
    -lole32 -luuid -lshell32 -ladvapi32 -luser32 -static-libgcc -static-libstdc++ -static -s
```

## 用法

```bat
FastShutdown.exe            :: 创建快捷方式并弹窗提示
FastShutdown.exe /silent    :: 静默模式，仅失败时弹窗（便于脚本调用）
```

退出码：`0` 成功，`1` COM 初始化失败，`2` 创建快捷方式失败。

## 实现说明

| 环节 | 实现 |
| --- | --- |
| 桌面路径 | `RegQueryValueExW` 读取 `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders\Desktop`，`REG_EXPAND_SZ` 交由 `ExpandEnvironmentStringsW` 展开；失败时回退 `SHGetKnownFolderPath(FOLDERID_Desktop)` |
| 快捷方式 | `IShellLinkW` 设置目标、参数、工作目录、图标，再经 `IPersistFile::Save` 落盘（替代原 Python 版的 `WScript.Shell` COM 调用） |
| 系统目录 | `GetSystemDirectoryW`，不硬编码盘符 |
| 提示 | `MessageBoxW`，成功为信息图标，失败附带 `HRESULT` 码与系统错误描述 |
| 清单 | `app.manifest` 声明 `asInvoker` 权限与 Per-Monitor V2 DPI 感知，经 `app.rc` 以 `RT_MANIFEST` 嵌入 |

源文件：

```
main.cpp            程序实现
app.rc              资源脚本（图标 + 清单）
resource.h          资源 ID
app.manifest        应用程序清单
assets/icon.ico     可执行文件图标
build.bat           构建脚本
```

## ⬇️ 下载发行版

前往 [Releases](https://github.com/NeetheCheeBao/FastShutdown/releases) 页面下载

## ⚖️ 许可证

本项目采用 MIT 许可证 - 详情请参阅 [LICENSE](LICENSE) 文件。
