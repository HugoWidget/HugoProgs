# HugoMount

## 项目简介
HugoMount 是基于 C++ 开发的 Windows 平台虚拟磁盘管理工具，核心用于**虚拟磁盘的挂载、卸载、信息查询**，并支持快速挂载配置文件（swvdisk.vhd）/日志文件（swvdisk_log.vhd）对应的虚拟磁盘分区。工具强制要求管理员权限运行，提供丰富的命令行操作方式，适用于 Windows 环境下虚拟磁盘的便捷管理。

## 核心功能
- **虚拟磁盘信息查询**：通过 `list` 命令列出系统中所有虚拟磁盘的完整信息；
- **灵活挂载能力**：
  - `mount` 命令：按磁盘ID+分区ID挂载虚拟磁盘，支持手动指定挂载盘符；
  - `conf/log` 命令：自动查找关联 `swvdisk.vhd/swvdisk_log.vhd` 的虚拟磁盘并挂载，支持指定盘符；
- **多方式卸载**
- **参数校验与容错**：校验盘符、磁盘ID/分区ID（数字）等参数有效性；

## 使用方法
### 前置要求
- 运行环境：Windows 系统；
- 权限要求：**必须以管理员身份运行**（工具自动校验，非管理员权限会直接退出）；
- 依赖说明：无需额外依赖，编译后的可执行文件可直接运行。

### 1. 命令行语法总览
工具仅支持命令行参数操作，无交互式界面，执行 `help` 命令可查看完整用法：

HugoMount.exe help 或 HugoMount.exe -h
#### 用法:
  - HugoMount list                - 列出所有虚拟磁盘信息
  - HugoMount mount <disk> <part> [drive] - 挂载虚拟磁盘分区
  - HugoMount unmount <drive>     - 按盘符卸载（如: unmount D）
  - HugoMount unmount <disk> <part> - 按磁盘/分区号卸载
  - HugoMount help                - 显示此帮助信息
  - HugoMount conf [drive]        - 挂载配置文件分区 (swvdisk.vhd)
  - HugoMount log [drive]         - 挂载日志文件分区 (swvdisk_log.vhd)

## 注意事项
- 挂载 conf/log 分区时，若系统中未找到关联 swvdisk.vhd/swvdisk_log.vhd 的虚拟磁盘，工具会输出 “未找到包含 XXX 的磁盘” 错误；
- 若挂载 / 卸载失败，优先检查：① 是否为管理员权限 ② 磁盘 ID / 分区 ID / 盘符是否有效 ③ 目标虚拟磁盘是否存在。

## 许可证
- 本项目采用 **GNU General Public License v3.0 (GPLv3)** 许可证开源，详见 LICENSE 文件。