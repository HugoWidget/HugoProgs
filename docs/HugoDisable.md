# HugoDisable

## 项目简介

- HugoDisable 是基于 C++ 开发的 Windows 平台工具，核心用于**通过修改系统注册表快速禁用/启用希沃服务（SeewoServiceAssistant.exe）**。工具支持命令行参数和交互式两种操作模式，操作结果实时反馈，适用于快速管理希沃服务的启停状态。

## 核心功能

- **注册表操作**：修改 `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\SeewoServiceAssistant.exe` 项下的 `Debugger` 键值，实现希沃服务的禁用/启用；
- **权限校验**：强制要求管理员权限运行，确保注册表写入/删除操作生效；

## 使用方法

### 前置要求

- 运行环境：Windows 系统（需支持注册表操作）；
- 权限要求：**必须以管理员身份运行**（工具会自动校验，非管理员权限会直接退出）。

### 1. 命令行模式（推荐）

- 直接通过命令行参数指定操作，无需交互，适合脚本/批量执行：

- 禁用希沃服务
  HugoDisable.exe -disable

- 启用希沃服务
  HugoDisable.exe -enable

- 查看帮助
  HugoDisable.exe -h

### 2. 直接交互

- 输入
  0:禁用希沃
  1:启用希沃

# 注意事项

- 工具仅针对 **SeewoServiceAssistant.exe** 服务生效，不影响希沃其他组件
- 该注册表受希沃文件保护，使用本项目前请先用**HugoProtect**项目关闭文件保护
- 若仍提示 “写入注册表失败”，需检查系统组策略 / 安全软件是否限制了注册表修改权限

# 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。
