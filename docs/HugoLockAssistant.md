# HugoLockAssistant

## 简介

HugoLockAssistant 是一个 Windows 启动辅助程序，用于根据命令行参数动态启动不同的 Hugo 相关工具。它会根据 `--method` 和 `--mode` 的取值，将特定的命令行参数透传给被调用的程序。

## 使用方法

```bash
HugoLockAssistant.exe --method=<method> --mode=<mode> [--extracmd=<额外参数>]
```

### 参数说明

| 参数         | 必填 | 可选值                                  | 说明                                                         |
| ------------ | ---- | --------------------------------------- | ------------------------------------------------------------ |
| `--method`   | 是   | `dbg`, `launchtool`, `frontend`, `lock` | 指定要启动的目标程序                                         |
| `--mode`     | 是   | `assist` 或 `direct`                    | 指定运行模式，根据 `method` 的不同会转换成不同的实际参数     |
| `--extracmd` | 否   | 任意字符串                              | 附加的命令行参数，会原样追加到最终命令行末尾（会先自动添加一个空格） |

## 启动映射与参数传递规则

| `--method` 值 | 启动的目标程序       | `--mode=assist` 时传递的实际参数 | `--mode=direct` 时传递的实际参数 |
| ------------- | -------------------- | -------------------------------- | -------------------------------- |
| `dbg`         | `HugoDbg.exe`        | `--lockfile=delete`              | `--lockfile=create`              |
| `launchtool`  | `HugoLaunchTool.exe` | `--kill`                         | `--stop`                         |
| `frontend`    | `HugoFrontend.exe`   | `--mode=assist`                  | `--mode=direct`                  |
| `lock`        | `HugoLock.exe`       | `--mode=assist`                  | `--mode=direct`                  |

> 注意：以上所有情况均支持追加 `--extracmd` 参数。例如，若同时指定 `--extracmd="--verbose"`，则最终命令行会在上述基础参数后附加 ` --verbose`。

## 示例

### 调试工具 辅助模式

```cmd
HugoLockAssistant.exe --method=dbg --mode=assist
```

### 启动工具 直接模式

```cmd
HugoLockAssistant.exe --method=launchtool --mode=direct
```

### 启动前端，并额外附加自定义参数

```cmd
HugoLockAssistant.exe --method=frontend --mode=assist --extracmd="--config custom.ini"
```

### 启动HugoLock进程 “直接”模式

```cmd
HugoLockAssistant.exe --method=lock --mode=direct
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

- 程序不显示任何窗口（无控制台或 GUI 界面），仅在出错时弹出消息框。
- 启动目标程序后，本程序立即退出，不会等待子进程结束。
- 目标程序必须与 `HugoLockAssistant.exe` 位于同一目录，否则会失败。
- 命令行解析器（`CmdParser`）使用宽字符，支持 `/` 或 `--` 前缀，参数名不区分大小写。

## 许可

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。