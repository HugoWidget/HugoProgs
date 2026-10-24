# HugoFreezeApi

## 项目简介

HugoFreezeApi 通过 `IHugoFreeze` 接口与希沃冰点 API 通信，用于**设置、查询、尝试冻结磁盘**。程序提供交互式菜单，并在菜单顶部显示当前读取到的目标 IP 和端口。

## 核心功能

- **设置冻结磁盘**：`SetFreezeState`，传入磁盘标识，`0` 表示解除全部；
- **查询冻结状态**：`GetFreezeState`；
- **尝试冻结磁盘**：`TryProtect`，传入磁盘标识，`0` 表示解除全部；
- **配置自动读取**：启动时调用 `GetConfig(ip, port)`，在交互菜单中显示目标地址；
- **结果与日志**：成功/失败信息输出到控制台，并写入 `HugoFreezeApi.log`。

## 使用方法

### 前置要求

- 运行环境：Windows；
- 程序本身不强制管理员权限；实际 API 调用权限由冰点服务/服务端决定。

### 命令行

```text
HugoFreezeApi.exe --set-vol <磁盘标识>
HugoFreezeApi.exe --get-disk
HugoFreezeApi.exe --try-protect <磁盘标识>
HugoFreezeApi.exe --help, -h
```

- `--set-vol`：设置冻结磁盘，`0` 表示解除全部；
- `--get-disk`：查询当前冻结状态；
- `--try-protect`：尝试冻结指定磁盘，`0` 表示解除全部；
- 缺少参数时输出错误并返回非 0；
- 未知选项输出提示并返回 1。

### 交互菜单

无参数运行进入：

```text
=== 希沃冰点Api通信工具 ===
目标 IP: <ip>:<port>

1. 设置冻结磁盘
2. 查询冻结状态
3. 尝试冻结磁盘
0. 退出程序
```

- 菜单 1：输入磁盘标识（`0` 解除全部）；
- 菜单 2：直接查询；
- 菜单 3：输入磁盘标识（`0` 解除全部）；
- 菜单 0：退出。

## 注意事项

- 程序本身不校验“冻结必须包含 C 盘”；据使用经验，冻结请求通常需要包含系统盘，否则可能被服务端拒绝，具体以实际 API 返回为准；
- 操作结果以 `FrzOR::Success` 判断，失败时输出 `errMsg`；
- 交互菜单顶部显示的 IP/端口来自 `GetConfig`，若读取失败则显示为空。

## 日志

写入程序目录下的 `HugoFreezeApi.log`，记录设置、查询、尝试冻结的结果与错误信息。

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。
