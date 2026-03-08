# HugoLaunchTool

## 项目简介
- HugoLaunchTool 是基于 C++ 开发的 Windows 平台希沃（Seewo）进程/服务管理工具，核心用于**快速启动希沃核心服务或终止指定的希沃相关进程**。工具强制要求管理员权限运行，仅支持命令行参数操作，适用于便捷管控希沃程序的启停状态。

## 核心功能
- **希沃服务启动**：通过 `-start` 参数触发，启动前自动终止所有 `HugoLaunchTool.exe` 进程，避免冲突，随后尝试启动 `SeewoCoreService` 系统服务；
- **希沃进程终止**：通过 `-stop` 参数触发，监控并强制终止指定希沃进程（`SeewoServiceAssistant.exe`、`SeewoCore.exe`、`SeewoAbility.exe`、`SeewoLauncherGuard.exe`）；
- **权限强制校验**：启动时自动校验管理员权限，非管理员权限直接退出，确保服务/进程操作具备足够权限。

## 使用方法
### 前置要求
- 运行环境：Windows 系统；
- 权限要求：**必须以管理员身份运行**；
- 依赖说明：无需额外依赖，编译后的可执行文件可直接运行。

### 命令行操作
#### 1. 启动希沃服务
- HugoLaunchTool.exe -start

#### 2. 终止希沃相关进程
- HugoLaunchTool.exe -stop

## 注意事项
- -stop 参数的进程终止操作是持续的，只要程序运行就会持续检测并终止目标进程；

## 许可证
- 本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。