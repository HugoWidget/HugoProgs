# HugoLockAssistant

## 简介

HugoLockAssistant 是一个希沃锁屏辅助程序，用于根据命令行参数动态启动不同的相关工具。它会根据 `--method` 和 `--mode` 的取值，将特定的命令行参数透传给被调用的程序。

默认情况下，程序会首先弹出一个预览对话框，显示即将执行的命令行，你可以选择复制到剪贴板、直接启动或取消。若不需要该确认步骤，可添加 `--launch` 参数直接启动目标程序。

## 使用方法

```bash
HugoLockAssistant.exe --method=<method> --mode=<mode> [--extracmd=<额外参数>] [--hide] [--launch]
```

### 参数说明

| 参数         | 必填 | 可选值                                  | 说明                                                         |
| ------------ | ---- | --------------------------------------- | ------------------------------------------------------------ |
| `--method`   | 是   | `dbg`, `launchtool`, `frontend`, `lock` | 指定要启动的目标程序                                         |
| `--mode`     | 是   | `assist`, `direct`, `disable`           | 指定运行模式，根据 `method` 的不同会转换成不同的实际参数     |
| `--extracmd` | 否   | 任意字符串                              | 附加的命令行参数，会原样追加到最终命令行末尾（会先自动添加一个空格） |
| `--hide`     | 否   | 无                                      | 决定是否隐藏目标程序的窗口（存在该参数则隐藏）               |
| `--launch`   | 否   | 无                                      | 跳过预览对话框，直接启动目标程序                             |

若 **未指定** `--launch`，程序会弹出一个信息对话框，内容包含：
- 提示信息：不建议直接通过该工具启动子工具；
- 即将执行的完整命令行；
- 三个按钮：
  - **中止**：将命令行复制到系统剪贴板，然后退出；
  - **重试**：直接启动目标程序（相当于临时决定执行）；
  - **忽略**：取消操作，程序直接退出。

## 启动映射与参数传递规则

| `--method` 值 | 启动的目标程序       | `--mode=assist` 时传递的实际参数 | `--mode=direct` 时传递的实际参数 | `--mode=disable` 时传递的实际参数 |
| ------------- | -------------------- | -------------------------------- | -------------------------------- | --------------------------------- |
| `dbg`         | `HugoDbg.exe`        | `--fso=assist`                   | `--fso=direct`                   | `--fso=disable`                   |
| `launchtool`  | `HugoLaunchTool.exe` | `--kill`                         | `--stop`                         | `--stop`                          |
| `frontend`    | `HugoFrontend.exe`   | `--mode=assist`                  | `--mode=direct`                  | `--mode=disable`                  |
| `lock`        | `HugoLock.exe`       | `--mode=assist`                  | `--mode=direct`                  | `--mode=disable`                  |

> 注意：以上所有情况均支持追加 `--extracmd` 参数。例如，若同时指定 `--extracmd="--verbose"`，则最终命令行会在上述基础参数后附加 ` --verbose`。

## 示例

### 直接启动 HugoDbg（跳过对话框）

```cmd
HugoLockAssistant.exe --method=dbg --mode=assist --launch
```

### 预览模式启动 HugoLaunchTool

```cmd
HugoLockAssistant.exe --method=launchtool --mode=direct
```

### 隐藏窗口直接启动 HugoLock

```cmd
HugoLockAssistant.exe --method=lock --mode=direct --hide --launch
```

## 依赖

- 目标程序（`HugoDbg.exe`、`HugoLaunchTool.exe`、`HugoFrontend.exe`、`HugoLock.exe`）必须位于与 `HugoLockAssistant.exe` 相同的目录下。
- 程序调用 `RunExternalProgram` 启动目标，使用 `open` 操作，不会等待子进程结束。

## 错误处理

- 若缺少 `--method` 或 `--mode` 参数，会弹窗提示语法错误并退出。
- 若 `--method` 值不是 `dbg`、`launchtool`、`frontend`、`lock` 之一，会弹窗提示有效值范围。
- 若对应的目标程序文件不存在（例如目录下缺少 `HugoDbg.exe`），会弹窗报告完整路径错误。
- 若启动目标程序失败（`RunExternalProgram` 返回值 ≤ 32），会弹窗显示系统错误码。

## 注意事项

- 若未使用 `--launch`，程序会显示一个预览对话框，此时可根据需要选择复制命令行、直接启动或取消。
- 启动目标程序后，本程序立即退出，不会等待子进程结束。
- 目标程序必须与 `HugoLockAssistant.exe` 位于同一目录，否则会失败。

## 许可

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。