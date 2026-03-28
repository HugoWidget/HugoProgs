# HugoLockAssistant

## 项目简介
HugoLockAssistant 是基于 C++ 开发的 Windows 平台希沃管家窗口监控与进程管控辅助工具，核心用于**自动化监控希沃管家窗口状态并联动管控 HugoLock.exe 进程及注入的 DLL**。工具强制要求管理员权限运行，启动时自动运行 HugoLock.exe，循环检测前台的希沃管家全屏/置顶窗口，当窗口稳定存在时终止 HugoLock.exe、卸载 SeewoServiceAssistant.exe 中的 HugoHook.dll，随后重启 HugoLock.exe，形成闭环管控，适用于希沃管家窗口的自动化运维与进程注入管理。

## 核心功能
- **权限与实例管控**
- **自动启动依赖进程**：启动时自动运行同目录下的 `HugoLock.exe`，无需手动启动；
- **窗口状态检测**
- **进程与 DLL 管控**：
  - 窗口稳定触发后，终止 `HugoLock.exe` 进程；
  -  卸载所有 `SeewoServiceAssistant.exe` 进程中注入的 `HugoHook.dll`（同目录）；
  - 进程终止、DLL 卸载完成后，自动重启 `HugoLock.exe`，恢复窗口管控能力；
- **无限循环监控**：程序启动后无交互，后台无限循环执行窗口检测与进程管控逻辑，持续保障管控效果。

## 使用方法
### 前置要求
- 运行环境：Windows 系统；
- 权限要求：**必须以管理员身份运行**（窗口状态检测、进程终止、DLL 卸载均需管理员权限）；
- 依赖要求：
  - 程序同目录必须存在 `HugoLock.exe`；
  - 程序同目录必须存在 `HugoHook.dll`；
- 进程依赖：DLL 卸载功能仅针对 `SeewoServiceAssistant.exe` 进程，无此进程则卸载步骤自动跳过。

### 运行程序
1. 确认程序同目录存在 `HugoLock.exe` 和 `HugoHook.dll`；
2. 右键点击 `HugoLockAssistant.exe`，选择「以管理员身份运行」；
3. 终止程序：再次打开程序，或通过任务管理器结束 `HugoLockAssistant.exe` 进程。

## 注意事项
- 权限相关：
  - 非管理员权限运行会直接退出，无法执行任何窗口检测、进程管控操作；
  - 调试权限不足可能导致 `HugoHook.dll` 卸载失败，但不影响 `HugoLock.exe` 终止与重启；
- 文件依赖：
  -  HugoLock.exe`；
  -  HugoHook.dll ；
- 进程管控逻辑：
  - 触发管控后会终止 `HugoLock.exe` 进程，若存在多实例 `HugoLock.exe` 均会被终止；
- 终止方式：程序无退出指令，需手动结束进程，强制终止不会导致系统异常。

## 许可证
本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。