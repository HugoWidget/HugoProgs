# HugoProgs 用户文档

> 面向使用者：安装、菜单操作、开机自启动、`.hps` 脚本与命令参考。
>
> 开发者请阅读 [HugoProgs 开发者文档](./Developer.md)。

## 目录

- [1. 项目简介](#1-项目简介)
- [2. 获取与安装](#2-获取与安装)
- [3. 运行要求](#3-运行要求)
- [4. 快速上手](#4-快速上手)
- [5. 界面与交互规则](#5-界面与交互规则)
- [6. 命令参考](#6-命令参考)
- [7. 配置与开机自启动](#7-配置与开机自启动)
- [8. `.hps` 脚本](#8-hps-脚本)
- [9. 日志](#9-日志)
- [10. 命令行直接调用](#10-命令行直接调用)

## 1. 项目简介

HugoProgs 由多个子项目组成，同名子项目作为菜单驱动的控制中调用其他子工具来实现一站式管理。

如果需要图形界面，见 [HugoWidgets](https://github.com/HugoWidget/HugoWidgets)。

## 2. 获取与安装

### 推荐：使用 HugoSetup 安装包

前往 [HugoSetup](https://github.com/HugoWidget/HugoSetup) 获取已配置好的 `HugoProgs-vx.x.x-bundle.zip`。该 bundle 由 [HugoProgs](https://github.com/HugoWidget/HugoProgs) 与 [WinTools](https://github.com/howdy213/WinTools) 组成，包含：

| 文件                       | 作用                                                         |
| -------------------------- | ------------------------------------------------------------ |
| `Start.exe`                | 以管理员模式启动 `HugoProgs.exe`                             |
| `Install.exe`              | 注册开机自启动（等价于菜单 `config/service/install`），会注册自启 HugoLock / HotspotHelper，即注入希沃管家解除锁屏与热键并启动班级热点 |
| `UnInstall.exe`            | 取消开机自启（等价于 `config/service/uninstall`）            |
| `HugoProgs.exe` 及各子工具 | 主程序与全部子工具                                           |

### 或：自行编译

见 [HugoProgs 开发者文档](./Developer.md) 的构建章节。仓库 Release 中的 `HugoProgs.zip` 为该项目完整编译产物（不含其他工具）。

## 3. 运行要求

- **操作系统**：Windows 10 / 11（子工具多为 x64）
- **权限**：**建议全程以管理员身份运行**。启动时程序会检测管理员权限，非管理员下会提示「当前无管理员权限，部分功能可能无法正常运行」；虚拟磁盘管理、文件保护等功能依赖管理员权限。
- **目录约定**：`HugoProgs.exe` 必须与**所有依赖的子工具 exe 位于同一目录**。程序调用子工具时按当前进程目录查找，找不到会输出「错误：未找到程序文件 <路径>」。
- 若使用 `.hps` 脚本并含中文，建议将脚本编码处理为不含中文，避免解析问题（见 [8. `.hps` 脚本](#8-hps-脚本)）。

## 4. 快速上手

1. 解压 bundle 到一个固定目录（建议路径不含中文与空格）。
2. 双击 `Start.exe`，或右键 `HugoProgs.exe` → 以管理员身份运行。
4. 在主菜单中执行操作。
5. 需要自动化时，编辑 `Launcher.ini` 并注册自启动服务（见第 7 节）。

## 5. 界面与交互规则

### 通用命令

在任意菜单层级均可使用的命令（输入 `common` 可查看全部通用命令，很大一部分用于脚本操作，可忽略）

### 快捷输入

| 形式      | 含义                                               |
| --------- | -------------------------------------------------- |
| `c<数字>` | 执行当前菜单下第 N 个命令（按显示顺序）。例如 `c1` |
| `s<数字>` | 进入当前菜单下第 N 个子菜单。例如 `s2`             |
| `..`      | 返回上一级                                         |

### 命令格式

```
<命令路径> [参数1] [参数2] ...
```

- 命令名**大小写敏感**，需按注册时的原样书写。
- 参数用空格分隔，数量视具体命令而定。
- 命令路径可以用 `/` 连接层级，例如 `mount/mnt 0 1 Z`、`basic/disable`、`freeze/drv.get`。

## 6. 命令参考

以下是主界面的一级子菜单（显示顺序即实际菜单顺序）：

### 6.1 `mount` — 希沃虚拟磁盘管理器

> `browse.cli` / `browse.gui` 需要发布包中存在 `CExplorer.exe` / `PyExplorer.exe`，否则会提示未找到程序文件。

### 6.2 `freeze` — 希沃冰点配置工具

HugoWidget 提供多种管理冰点还原的方式，可按需取用：

| 工具     | 介绍                                | 特点               |
| -------- | ----------------------------------- | ------------------ |
| Api      | 与希沃冰点服务交互                  | 安全性最高         |
| Driver   | 与希沃冰点驱动交互                  | 速度最快，功能最全 |
| File     | 修改本地冰点文件                    | 高度自定义         |
| Hook     | 驱动运行时内存篡改 + 内核执行流劫持 | 支持动态修改       |
| Disk     | 驱动级删除配置                      | 通用性强           |
| Official | 官方配置修改工具                    | 原生体验           |
| WinPE    | 在 WinPE 中删除配置文件             | 通用性强           |

### 6.3 `lock` — 希沃锁屏工具

该子菜单下只有 `info`，用于提示：**锁屏类工具不适合在此直接调用**，请使用 `execute` 命令手动执行，具体用法见对应工具文档（[HugoLock](./HugoLock.md)、[HugoLockAssistant](./HugoLockAssistant.md)），该工具请使用 `Launcher` 来实现自启。

### 6.4 `winpe` — WinPE 工具

> 需要发布包中存在 `HugoWinPE\PEOutside.exe`；该工具用于自动配置 BCD 启动项、进入 WinPE 后执行解除冰点等维护动作，详见 [HugoWinPE](https://github.com/HugoWidget/HugoWinPE)。

### 6.5 `execute` — 执行外部程序

| 命令     | 说明                                                         |
| -------- | ------------------------------------------------------------ |
| `open`   | 以普通权限打开 `<路径> [参数...]`                            |
| `runas`  | 以管理员权限运行 `<路径> [参数...]`                          |
| `create` | 用 `CreateProcess` **阻塞**打开 `<路径> [参数...]`（等待程序退出） |

### 6.6 `config` — 配置与自启动

| 命令                | 说明                                                    |
| ------------------- | ------------------------------------------------------- |
| `auto`              | 用默认编辑器打开 `Launcher.ini`（自启动配置）           |
| `task`              | 用默认编辑器打开 `tasks.ini`（任务计划配置）            |
| `service/install`   | 安装自启动服务（`AutoStartService.exe /install`）       |
| `service/uninstall` | 卸载自启动服务（`AutoStartService.exe /uninstall`）     |
| `service/query`     | 查询服务状态（输出服务路径，并显示「已注册 / 未注册」） |

## 7. 配置与开机自启动

### 7.1 自启动链路

```text
AutoStartService 服务启动
└--> Launcher 启动 -----> 依次调用各种程序
      |
      | 若有 TaskManager
      |
      └-----> 则按配置在指定时刻调用程序
```

注册自启动服务实质是由 `AutoStartService.exe` 启动 `Launcher.exe` 而非 `HugoProgs.exe`：开机后由服务拉起 Launcher，Launcher 再按 `Launcher.ini` 逐个打开目标程序。

### 7.2 `Launcher.ini` 配置

在菜单 `config/auto` 打开该文件（位于程序目录）。格式为一组任意英文节名，每节描述一个要启动的程序：

```ini
[节名，任意的英文字符串，如Pro1]
Program=.\HugoLock.exe
Params=--mode=assist
RunAsAdmin=true
ShowWnd=0

[不与上面重复的节名]
Program=.\HotspotHelper.exe
Params=-start
RunAsAdmin=true

[Pro3]
Program=.\TaskManager.exe

...
```

字段含义：

| 字段         | 说明                                              |
| ------------ | ------------------------------------------------- |
| `Program`    | 目标程序路径（相对程序目录，如 `.\HugoLock.exe`） |
| `Params`     | 传给该程序的命令行参数                            |
| `RunAsAdmin` | 是否以管理员权限启动（`true` / `false`）          |
| `ShowWnd`    | 是否显示窗口（`0` 为不显示）                      |

按需增删节即可定制开机行为。结合各子工具的使用说明修改该文件，即可实现不同的开机功能组合。

### 7.3 `tasks.ini` 与 TaskManager

`Launcher.ini` 默认配置中包含 `.\TaskManager`，配合 `tasks.ini`（菜单 `config/task` 打开）实现**按指定时刻调用程序**的能力。其配置方法类似于 `Launcher.exe`，参见 [WinTools](https://github.com/howdy213/WinTools)。若不需要定时任务，可以把对应节从 `Launcher.ini` 中移除。

### 7.4 注册 / 查询 / 卸载服务

```text
config/service/install     安装自启动服务
config/service/query       查询服务状态
config/service/uninstall   卸载自启动服务
```

或直接使用 bundle 中的 `Install.exe` / `UnInstall.exe`。

## 8. `.hps` 脚本

完整语法见项目文档 [hps.md](./hps.md)。

## 9. 日志

HugoProgs 及其子工具的日志由 `HugoLogs.exe` 统一管理，可通过 `logs` 子菜单操作：

- `logs ls`：列出所有日志
- `logs clr`：清除所有日志
- `logs run`：打开 `HugoLogs` 交互界面
- `logs hlp`：查看帮助

子工具的详细日志说明见 [HugoLogs 文档](./HugoLogs.md)。

## 10. 命令行直接调用

除交互式菜单外，`HugoProgs.exe` 也支持直接执行命令与脚本：

```text
HugoProgs.exe                    进入交互式菜单
HugoProgs.exe <命令> [参数...]    直接执行指定命令
HugoProgs.exe --help           显示帮助信息
HugoProgs.exe <脚本.hps>         执行脚本文件
```

### 免责声明

本项目仅用于研究或教育目的，请勿将本项目用于可能违反当地法律、侵犯著作权或其他软件 EULA 的用途。若将本项目用于非法用途，一切后果由使用者承担，开发者不承担此类行为带来的任何后果或责任。