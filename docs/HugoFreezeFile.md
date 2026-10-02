# HugoFreezeFile

## 项目简介

HugoFreezeFile 是一个交互式 `VolumeInfo.config` 编辑器，用于查看和修改希沃磁盘冻结配置文件。程序基于 HugoUtils 的冻结后端（`HFreezeFileBackend` / `HConfigFile`），可读取、编辑、校验并保存 `VolumeInfo.config`，保存或修改字段后会自动重算配置 MD5。

程序运行后进入交互式菜单，适合手工检查冻结配置、修改冻结卷掩码或调整单个 `ProtectInfo` 字段。

可以使用 `HugoFreezeDriver` 来导入配置。

## 功能概览

- 启动时尝试加载系统默认配置 `VolumeInfo.config`，失败则创建空白配置；
- 以可读形式查看 `ProtectInfo` 全部字段；
- 以十六进制二进制转储查看完整配置，并标注 `ProtectInfo` 区域；
- 显示配置中存储的 MD5，并重新计算 MD5 进行校验，提示 `VALID` 或 `MISMATCH`；
- 通过简化方式修改冻结卷掩码，并自动重算 MD5；
- 通过高级菜单修改单个字段；
- 支持打开、新建、保存、另存为配置文件；
- 保存 / 另存为时自动重算 MD5，保证配置自洽；
- 未保存更改时，在退出、打开或新建前进行确认；
- 对定长字符串字段按字段长度截断，避免越界。

## 命令行用法

```text
HugoFreezeFile.exe
```

| 选项   | 说明                     |
| ------ | ------------------------ |
| 无参数 | 进入交互式菜单           |
| 无     | 当前版本不解析命令行选项 |

交互菜单：

```text
1. View current configuration
2. View binary dump
3. Modify freeze volume mask (simplified)
4. Modify individual field (advanced)
5. Save
6. Save as
7. Open
8. New
9. Exit
```

示例：

```bat
HugoFreezeFile.exe
```

## 注意事项

- 修改 `VolumeInfo.config` 可能影响冻结 / 解冻行为，操作前建议备份原文件；
- 若目标配置文件位于受保护系统目录，可能需要以管理员身份运行；
- 保存时会重算 MD5，若手工编辑或外部修改可能导致 MD5 不匹配；
- 冻结 / 解冻相关配置通常需要重启或由驱动重新加载后生效；
- 本工具不会自动备份目标配置，请自行保留副本。

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。
