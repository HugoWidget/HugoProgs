# HugoMount

## 项目简介

HugoMount 是 Windows 平台虚拟磁盘管理工具，核心用于**虚拟磁盘的挂载、卸载、信息查询**，并支持按文件名自动定位配置文件分区（`swvdisk.v`）和日志文件分区（`swvdisk_log.v`）对应的虚拟磁盘。工具要求管理员权限运行，支持命令行和交互式菜单两种方式。

## 核心功能

- **虚拟磁盘信息查询**：`--list` 列出系统中所有虚拟磁盘的完整信息；
- **按磁盘/分区挂载**：`--mount <disk> <part> [drive]`，可指定挂载盘符；
- **按盘符或磁盘/分区卸载**：`--unmount <drive>` 或 `--unmount <disk> <part>`；
- **配置文件分区挂载**：`--conf [drive]`，自动查找 backing file 路径包含 `swvdisk.v` 的虚拟磁盘并挂载其第一个分区；
- **日志文件分区挂载**：`--log [drive]`，自动查找 backing file 路径包含 `swvdisk_log.v` 的虚拟磁盘并挂载其第一个分区；
- **参数校验**：盘符必须是单个字母；`--mount` / `--unmount` 的磁盘/分区号按整数解析；
- **管理员权限与单实例**：启动时强制校验管理员权限，并启用单实例保护。

## 使用方法

### 前置要求

- 运行环境：Windows；
- 权限要求：**必须以管理员身份运行**（`RequireAdminPrivilege(true)`）；
- 单实例：同一时间只允许一个实例运行（`EnsureSingleInstance(true)`）。

### 命令行语法

```text
HugoMount.exe --list
HugoMount.exe --mount <disk> <part> [drive]
HugoMount.exe --unmount <drive>
HugoMount.exe --unmount <disk> <part>
HugoMount.exe --conf [drive]
HugoMount.exe --log [drive]
HugoMount.exe --help, -h
```

说明：

- 参数前缀可省略（`CmdParser` 会把 `--list` 归一化为 `list`），因此 `HugoMount.exe list` 等价于 `--list`；
- `drive` 必须是单个字母，大小写不敏感，内部转为大写；
- `--conf` / `--log` 会自动定位磁盘和第一个分区，无需手动输入磁盘/分区号。

### 交互式菜单

无参数运行进入：

```text
=== 虚拟磁盘挂载工具 ===
1. 列出所有虚拟磁盘信息
2. 挂载虚拟磁盘分区
3. 卸载虚拟磁盘
4. 挂载配置文件分区
5. 挂载日志文件分区
0. 退出程序
```

- 菜单 2：依次输入磁盘编号、分区编号、盘符（可选，直接回车跳过）；
- 菜单 3：选择 `a` 按盘符卸载，或 `b` 按磁盘/分区号卸载；
- 菜单 4 / 5：调用 `--conf` / `--log` 的等价逻辑。

### 配置/日志分区定位规则

- `--conf`：在 `GetAllInfo()` 返回的所有磁盘 backing files 中查找路径包含 `swvdisk.v`（不区分大小写）的磁盘，取其第一个分区；
- `--log`：查找路径包含 `swvdisk_log.v` 的磁盘，取其第一个分区。

> 注意文件名是 `swvdisk.v` 与 `swvdisk_log.v`，不是 `.vhd`。

## 注意事项

- 挂载 `conf` / `log` 时，若未找到包含目标文件的磁盘，输出“未找到包含 XXX 的磁盘”；
- 若目标磁盘没有有效分区，输出“磁盘 N 没有有效分区可挂载”；
- 挂载 / 卸载失败时优先检查：
  1. 是否以管理员身份运行；
  2. 磁盘 ID / 分区 ID / 盘符是否有效；
  3. 目标虚拟磁盘是否存在。

## 日志

写入程序目录下的 `HugoMount.log`，记录各命令执行结果与错误信息。

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。
