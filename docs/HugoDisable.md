# HugoDisable

## 项目简介

HugoDisable 是基于 C++ 开发的 Windows 平台工具，用于**管理希沃（Seewo）相关进程和服务的运行状态**。它通过修改系统注册表（IFEO）禁用/启用希沃服务，并通过 Windows 防火墙规则阻止/允许希沃更新程序及核心进程的网络访问。工具支持命令行参数和交互式菜单两种操作模式，需要管理员权限运行。

## 核心功能

- **服务禁用/启用**：修改注册表 `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\SeewoServiceAssistant.exe` 下的 `Debugger` 键值，实现希沃服务的快速禁用或恢复。
- **更新程序网络控制**：通过防火墙阻止或允许希沃更新程序（`EasiUpdate3.exe`、`C:\ProgramData\Seewo\Easiupdate\easiupdate\EasiUpdate.exe`）的网络访问。
- **核心进程网络控制**：通过防火墙阻止或允许希沃核心进程（`SeewoCore.exe`、`SeewoServiceAssistant.exe`）的网络访问。
- **清理防火墙规则**：一键删除所有由本工具创建的希沃相关防火墙规则。
- **权限校验**：强制要求管理员权限运行，确保注册表和防火墙操作生效；同时确保单实例运行。

## 使用方法

### 前置要求

- 权限要求：**必须以管理员身份运行**（工具会自动校验，非管理员权限会直接退出）。

### 1. 命令行模式

在命令提示符或 PowerShell 中以管理员身份运行，通过参数指定操作，适合脚本或批量执行。

| 参数 | 说明 |
|------|------|
| `--disable` | 禁用希沃服务（通过 IFEO） |
| `--enable` | 启用希沃服务（删除 IFEO 项） |
| `--disable-update` | 阻止希沃更新程序的网络访问 |
| `--enable-update` | 允许希沃更新程序的网络访问 |
| `--disable-net` | 阻止希沃核心进程的网络访问 |
| `--enable-net` | 允许希沃核心进程的网络访问 |
| `--clear-rules` | 删除所有希沃相关的防火墙规则 |
| `--help` 或 `-h` | 显示帮助信息 |

**示例：**

```cmd
HugoDisable.exe --disable
HugoDisable.exe --disable-update
HugoDisable.exe --clear-rules
```

### 2. 交互式菜单

直接双击运行（或不带任何参数）将进入交互菜单，按数字键选择操作：

```
=== 希沃控制工具 ===
1. 禁用希沃服务
2. 启用希沃服务
3. 禁用希沃更新
4. 启用希沃更新
5. 禁用希沃网络
6. 启用希沃网络
7. 清除所有网络规则
0. 退出程序
请输入选择：
```

## 注意事项

- 工具仅针对希沃的特定进程（`SeewoServiceAssistant.exe`、`SeewoCore.exe`、`EasiUpdate3.exe` 等）生效，不影响其他组件。
- 注册表路径（IFEO）可能被希沃文件保护机制阻拦。使用本工具前，建议先使用 **HugoProtect** 项目关闭希沃的文件保护。
- 若提示“写入注册表失败”，请检查系统组策略或第三方安全软件是否限制了注册表修改权限。
- 防火墙规则的创建和删除需要 Windows 防火墙服务正在运行，且当前用户具有修改策略的权限。

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 [LICENSE](LICENSE) 文件。
