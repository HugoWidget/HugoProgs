# HugoLock

## 项目简介

HugoLock 是 Windows 平台下针对希沃（Seewo）教学软件的辅助工具，核心功能为**向 `SeewoServiceAssistant.exe` 注入 `HugoHSSA.dll`**，并通过共享内存标志 `HugoLockFlag` 与 DLL 协同工作，实现对“希沃管家”置顶窗口的隐藏控制。

程序支持三种运行模式：`direct`（默认，后台持续拦截）、`assist`（图形界面，手动控制）、`disable`（后台持续拦截并本地隐藏新出现的置顶窗口）。

程序要求管理员权限，并尝试启用 UIAccess；UIAccess 失败时仅降级功能，不退出。

## 核心功能

- **单实例保护**：`EnsureSingleInstance(true)`；
- **权限**：
  - `RequireAdminPrivilege(true)`：非管理员直接退出；
  - `UIAccess::RequireUIAccess()`：失败仅记录警告并继续；
  - `EnableDebugPrivilege()`：启用调试权限；
- **进程注入**：
  - 后台线程每 500ms 检查 `SeewoServiceAssistant.exe`，未注入则注入同目录下的 `HugoHSSA.dll`；
  - 线程 detach，主流程退出时随之结束；
- **共享标志**：创建名为 `HugoLockFlag` 的共享内存标志，`TRUE` 表示拦截，`FALSE` 表示不拦截；
- **三种模式**：
  - `direct`：`flag=TRUE`，循环保持，无界面；
  - `assist`：`flag=FALSE`，读取 `HugoLock.ini` 的 `UI` 配置，显示对话框，用户通过按钮控制；
  - `disable`：`flag=TRUE`，循环保持，并本地隐藏新出现的置顶“希沃管家”窗口。

## 使用方法

### 前置要求

- 权限：**必须以管理员身份运行**；
- 文件依赖：程序同目录下必须存在 `HugoHSSA.dll`；
- 可选配置：`HugoLock.ini`（Assist 模式首次运行会自动创建）。

### 命令行

```cmd
:: Direct 模式（默认）
HugoLock.exe

:: Assist 模式（图形界面）
HugoLock.exe --mode=assist

:: Disable 模式
HugoLock.exe --mode=disable

:: 显示帮助（不要求管理员权限）
HugoLock.exe --help
HugoLock.exe -h
HugoLock.exe /?
```

- `--mode` 可取 `direct`、`assist`、`disable`，缺省为 `direct`；
- 帮助分支在权限检查之前处理，因此 `--help` 不需要管理员权限。

### 模式说明

#### Direct 模式（默认）

- 设置 `flag=TRUE`；
- 进入 `DirectUnlockLoop()`，每 100ms 确保标志为 `TRUE`；
- 无用户界面，需通过任务管理器或关闭控制台结束。

#### Assist 模式

- 设置 `flag=FALSE`；

- 读取 `HugoLock.ini`：

  ```ini
  [Config]
  UI=dialog      ; 或 button
  ```

  - `dialog`：主对话框（功能完整）；
  - `button`：简化对话框（仅关闭按钮）；

- 对话框行为：

  - 每 1000ms 检测是否存在全屏“希沃管家”窗口，存在则显示对话框并置顶，否则隐藏；
  - 每 50ms 置顶对话框并更新按钮状态；
  - 主对话框：
    - “打开/解锁”按钮：启用拦截、执行一次 `UnlockOnce()`，按钮变为 ✓；
    - “Esc”按钮：点击 3 次后退出程序，2 秒无操作自动重置计数；
  - 简化对话框：点击关闭按钮后启用拦截并执行一次 `UnlockOnce()`，按钮变为 ✓；

- 关闭对话框即退出程序。

#### Disable 模式

- 设置 `flag=TRUE`；
- 进入 `DisableUnlockLoop()`，每 100ms：
  - 确保标志为 `TRUE`；
  - 调用 `UnlockWithRecord()`，隐藏新出现的置顶“希沃管家”窗口（已处理过的窗口记录在静态列表中，避免重复隐藏导致焦点异常）。

### 配置文件

`HugoLock.ini` 仅 Assist 模式使用，首次运行时自动创建，默认内容：

```ini
[Config]
UI=dialog
```

## 工作原理

1. 解析命令行，处理 `--help`；
2. 申请管理员权限、尝试 UIAccess、启用调试权限、单实例检查；
3. 创建共享标志 `HugoLockFlag`；
4. 启动后台注入线程（每 500ms 检查并注入 `HugoHSSA.dll`）；
5. 按模式分支：
   - `direct`：循环保持 `flag=TRUE`；
   - `assist`：显示对话框，用户交互控制标志；
   - `disable`：循环保持 `flag=TRUE` 并本地隐藏置顶窗口；
6. 注入的 DLL 读取共享标志，`TRUE` 时执行窗口隐藏/事件拦截。

## 注意事项

- 非管理员运行会直接退出；
- `HugoHSSA.dll` 缺失或注入失败时，共享标志仍可工作，但窗口控制会失效；
- Direct / Disable 模式无界面，需强制结束进程；
- Assist 模式正常关闭对话框即可退出；
- 注入系统进程可能被安全软件拦截，建议提前添加信任；
- `HugoLockAssistant` 已不再推荐使用，建议直接构造 `HugoLock.exe --mode=...` 命令行。

## 日志

写入程序目录下的 `HugoLock.log`，记录启动、UIAccess 状态、运行模式、共享标志与注入流程。

## 许可证

本项目使用 GNU General Public License v3.0 开源协议，详见 LICENSE 文件。
