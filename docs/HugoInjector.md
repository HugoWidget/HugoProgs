# HugoInjector

## 项目简介
HugoInjector 是基于 C++ 开发的 Windows 平台 DLL 注入/卸载工具，核心用于**将指定 DLL 注入目标进程，或从目标进程中卸载指定 DLL**。工具支持命令行参数操作，内置调试权限启用、单实例运行保护、进程监控自动注入机制，适用于 Windows 环境下的 DLL 注入/卸载管理。

## 核心功能
- **DLL 注入**：支持两种命令格式触发注入，自动监控目标进程并持续尝试注入指定 DLL；
- **DLL 卸载**：从所有匹配名称的目标进程中批量卸载指定 DLL；
- **权限提升**：自动启用系统调试权限，确保注入/卸载操作具备足够权限；
- **单实例保护**：基于 DLL 路径+目标进程名生成唯一标识，防止工具多开导致的操作冲突；

## 使用方法
### 前置要求
- 运行环境：Windows 系统；
- 权限要求：建议以**管理员身份运行**（调试权限启用依赖高权限，普通权限可能操作失败）；
- 路径要求：DLL 文件路径需准确（绝对路径/相对路径均可，推荐绝对路径避免解析错误）；
- 进程要求：目标进程名需包含后缀（如 `notepad.exe`），确保进程匹配准确。

### 1. 命令行语法说明
程序仅支持命令行参数操作，无交互式界面：

```text
HugoInjector.exe <DLL路径> <目标进程名>               注入并监视目标进程
HugoInjector.exe -inject <DLL路径> <目标进程名>       同上（显式指定 -inject）
HugoInjector.exe -uninject <DLL路径> <目标进程名>     从所有匹配进程中卸载 DLL
HugoInjector.exe --help, -h, /?                      显示帮助
```

示例：

```bat
HugoInjector.exe C:\test.dll notepad.exe
HugoInjector.exe -inject C:\test.dll notepad.exe
HugoInjector.exe -uninject C:\test.dll notepad.exe
```

参数不足时输出用法并返回非 0；参数错误时同样输出用法提示。

## 注意事项
- 调试权限启用失败会导致注入 / 卸载操作失败，建议始终以管理员身份运行程序；
- 注入操作会持续监控目标进程（间隔 100ms），直到程序手动终止，只要目标进程存在就会尝试注入；
- 卸载操作会作用于所有匹配进程名的进程，而非单个进程，需确认目标进程范围后再执行；
- 程序异常信息会直接输出到控制台，可根据提示排查路径错误、进程不存在、权限不足等问题。

## 日志

运行日志写入程序目录下的 `HugoInjector.log`，同时输出到控制台；可记录启动、注入/卸载目标以及异常信息。

## 许可证
本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。