# HugoLogs

HugoLogs 是一个简单的 Windows 命令行工具，用于查找和清理日志文件。  
它会在 `HugoLogs.exe` 所在目录下搜索所有以 `Hugo` 开头且以 `.log` 结尾的文件，同时还会检查当前用户目录下的 `HugoDbg.log`，并提供交互式或命令行方式查看、删除这些文件。

## 功能

- 自动查找程序所在目录下的 `Hugo*.log` 文件以及 `C:\Users\<用户名>\HugoDbg.log`
- 支持交互式菜单：
  - 列出所有匹配的文件，并可选择用记事本打开
  - 删除所有匹配的文件（有确认提示）
- 支持命令行参数，方便脚本调用：
  - `--list` / `-l` ：列出匹配的文件
  - `--delete` / `-d` ：直接删除所有匹配的文件（不询问）
  - `--help` / `-h` ：显示帮助信息

## 使用示例

### 交互模式
直接运行 `HugoLogs.exe`，会进入一个控制台菜单界面。

```
=== Hugo 日志文件清理工具 ===
程序目录: D:\HugoProgs
1. 列出匹配的文件 (Hugo*.log) 并可选择用记事本打开
2. 删除所有匹配的文件
0. 退出程序
请输入选择:
```

### 命令行模式

- 列出所有匹配的日志文件（不进入交互菜单）：
  ```bash
  HugoLogs.exe --list
  ```

- 直接删除所有匹配的日志文件（无确认）：
  ```bash
  HugoLogs.exe --delete
  ```

- 显示帮助：
  ```bash
  HugoLogs.exe --help
  ```

> 参数前缀可省略或替换：`--list`、`-l`、`/l` 均等价，`--delete`、`-d`，`--help`、`-h`、`/?` 同理。

## 常见日志文件

主程序与子工具会在程序目录写入各自的日志，例如 `HugoProgs.log`、`HugoMount.log`、`HugoFreezeDriver.log`、`HugoInjector.log`、`HugoBreak.log`、`HugoScreenSaver.log`、`HugoLock.log`、`HugoInfo.log` 等。

## 许可证

本项目基于 GNU General Public License v3.0 发布。  
