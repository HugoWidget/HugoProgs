# HugoBreak

## 项目简介

HugoBreak 是希沃管家 DLL 补丁工具，用于修补或还原 `bind_zmodule.dll`。程序通过查找特定字节序列并替换为补丁序列，实现对目标 DLL 的修改；还原操作从当前程序目录下的备份文件恢复。程序必须以管理员身份运行。

## 功能概览

- 启动时强制校验管理员权限；
- 自动定位希沃管家安装目录下的 `SeewoCore/module/bind/bind_zmodule.dll`；
- 修补前查找原始字节序列 `E8 16 E9 FF FF`，并替换为 `B0 01 90 90 90`；
- 首次修补时在程序当前目录创建 `bind_zmodule.dll.bak` 备份；
- 若文件已被修补，则提示无需重复修补；
- 支持从备份还原目标 DLL；
- 还原前检查备份是否包含原始序列，必要时提示确认；
- 支持命令行选项和交互式菜单。

## 命令行用法

```text
HugoBreak.exe [选项]
```

| 选项              | 说明                          |
| ----------------- | ----------------------------- |
| `--patch`, `-p`   | 修补 `bind_zmodule.dll`       |
| `--restore`, `-r` | 从备份还原 `bind_zmodule.dll` |
| `--help`, `-h`    | 显示帮助信息                  |
| 无参数            | 进入交互式菜单                |

示例：

```bat
HugoBreak.exe --patch
HugoBreak.exe --restore
HugoBreak.exe --help
HugoBreak.exe
```

交互菜单：

```text
1. 修补 bind_zmodule.dll
2. 还原 bind_zmodule.dll
0. 退出
```

## 注意事项

- 必须以管理员身份运行；
- 仅适用于包含指定原始字节序列的 `bind_zmodule.dll`，不同版本可能不兼容；
- 修改系统 DLL 存在风险，请务必保留备份；
- 若目标 DLL 被希沃服务占用，修补或还原可能失败，可先停止相关服务后重试；
- 备份文件默认保存在 `HugoBreak.exe` 所在目录，文件名为 `bind_zmodule.dll.bak`；
- 还原操作会覆盖目标 DLL，请确认备份来源可靠。

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。
