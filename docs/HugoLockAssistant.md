# HugoLockAssistant

## 简介

HugoLockAssistant 是一个 Windows 启动辅助程序，用于根据命令行参数动态启动不同的 Hugo 相关工具，并将工作模式（`--mode`）透传给被调用的程序。

## 使用方法

```bash
HugoLockAssistant.exe --method=<method> --mode=<mode>
```

### 参数说明

| 参数       | 必填 | 可选值                                  | 说明                             |
| ---------- | ---- | --------------------------------------- | -------------------------------- |
| `--method` | 是   | `dbg`, `launchtool`, `frontend`, `lock` | 指定要启动的目标程序             |
| `--mode`   | 是   | `assist` 或 `direct`                    | 指定运行模式，原样传递给目标程序 |

## 启动映射

| `--method` 值 | 启动的目标程序       | 传递的命令行参数 |
| ------------- | -------------------- | ---------------- |
| `dbg`         | `HugoDbg.exe`        | `--mode=<mode>`  |
| `launchtool`  | `HugoLaunchTool.exe` | `--mode=<mode>`  |
| `frontend`    | `HugoFrontend.exe`   | `--mode=<mode>`  |
| `lock`        | `HugoLock.exe`       | `--mode=<mode>`  |

## 示例

启动调试工具并进入“辅助”模式：

```cmd
HugoLockAssistant.exe --method=dbg --mode=assist
```

启动锁进程并进入“直接”模式：

```cmd
HugoLockAssistant.exe --method=lock --mode=direct
```

## 依赖

- 目标程序（`HugoDbg.exe`、`HugoLaunchTool.exe`、`HugoFrontend.exe`、`HugoLock.exe`）必须位于与 `HugoLockAssistant.exe` 相同的目录下。

## 错误处理

- 若缺少 `--method` 或 `--mode` 参数，会弹窗提示并退出。
- 若 `--method` 值非法，会提示有效值范围。
- 若对应的目标程序缺失（文件不存在），会弹窗报告路径错误。
- 若启动目标程序失败，会弹窗显示系统错误码。

## 注意事项

- 程序不显示任何窗口，仅在出错时弹出消息框。
- 启动目标程序后，本程序立即退出，不会等待子进程结束。
- 目标程序必须与 `HugoLockAssistant.exe` 位于同一目录，否则会失败。

## 许可

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。