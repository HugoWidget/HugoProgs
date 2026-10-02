# HugoInfo

## 项目简介

HugoInfo 是希沃管家信息查询工具，用于读取并显示希沃管家相关安装目录、版本、驱动路径、更新目录、MachineId 以及多个配置文件路径。程序无参数运行，显示信息后等待用户按键退出。

## 功能概览

- 查询 SeewoService 版本；
- 查询 SeewoService 安装目录；
- 查询 SeewoDriverService 目录；
- 查询 `DriverService.exe` 路径；
- 列出 Easiupdate3 更新目录及其数量；
- 查询 MachineId；
- 查询 `SeewoCore.ini` 路径；
- 查询 `SeewoLockConfig.ini` 路径；
- 查询 `.lock_backup` 路径；
- 查询 `school.ini` 路径；
- 对未找到的项显示 `[未找到]`。

## 命令行用法

```text
HugoInfo.exe
```

| 选项   | 说明                           |
| ------ | ------------------------------ |
| 无参数 | 直接查询并显示希沃管家相关信息 |

示例：

```bat
HugoInfo.exe
```

## 注意事项

- 本程序主要用于信息查询，不修改系统文件或配置；
- 部分路径或注册表信息可能因希沃管家版本、安装位置或权限不同而显示 `[未找到]`；
- 若需要读取受保护目录或注册表位置，建议以管理员身份运行；
- 输出结果依赖系统中已安装的希沃管家组件。

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。