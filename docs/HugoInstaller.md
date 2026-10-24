# HugoInstaller

## 项目简介

HugoInstaller 是希沃服务（SeewoService）安装/卸载管理工具：

- **安装**：扫描本地 `SetupPackage` 目录中的安装包，或从希沃下载接口下载指定版本/最新版本，下载完成后自动启动安装包；
- **卸载**：用 `HugoFakeVerify.exe` 替换 `uninstall_verify.exe`，然后启动官方卸载程序。

支持命令行和交互式菜单两种模式。

## 核心功能

### 安装

- 扫描程序目录下 `SetupPackage` 文件夹（不存在则创建），按文件名中的 `\d+\.\d+\.\d+\.\d+` 提取版本号；
- 菜单列出本地包，`[0]` 为下载新版本；
- 下载：
  - 输入版本号（格式 `\d+\.\d+\.\d+\.\d+`，`0` 表示最新）；
  - 临时文件为 `SetupPackage\SeewoServiceSetup.tmp`；
- 下载后处理：
  - 失败：输出错误和 HTTP 状态码；404 提示版本不存在或已下架；错误消息含 `7` 时提示可能已下载过；删除 0 字节临时文件；
  - 下载量小于 1,000,000 字节：删除临时文件并报“文件太小，版本可能错误”；
  - 重命名（目标已存在先删除），然后 `ShellExecute` 启动安装包。

### 卸载

- 要求程序目录存在 `HugoFakeVerify.exe`；
- 要求 `C:\Program Files (x86)\Seewo\SeewoService\Uninstall.exe` 存在；
- 读取 `HugoFakeVerify.exe` 全部字节；
- 备份原 `C:\Program Files (x86)\Seewo\SeewoService\uninstall_verify.exe` 到**同目录**下的 `OLD*.tmp`（`GetTempFileNameW`，不是系统临时目录）；
- 将 fake 内容写入 `uninstall_verify.exe`（`CREATE_ALWAYS`）；
- 写入失败时尝试用备份还原；
- 成功后 `ShellExecute` 启动 `Uninstall.exe`。

## 使用方法

### 前置要求

- 运行环境：Windows；
- 权限建议：以管理员身份运行（文件替换、目录创建、卸载操作）；
- 卸载依赖：
  - 程序同目录存在 `HugoFakeVerify.exe`；
  - 希沃默认安装路径存在 `Uninstall.exe`；
- 下载依赖：能访问 `BASE_URL` 指向的希沃下载接口。

### 命令行

```text
HugoInstaller.exe --install, -install
HugoInstaller.exe --uninstall, -uninstall
HugoInstaller.exe --help, -h
```

- `--install` / `-install`：进入安装流程（仍需在菜单中选择本地包或下载）；
- `--uninstall` / `-uninstall`：执行卸载流程；
- `--help` / `-h`：显示帮助；
- 未知选项返回 1。

### 交互菜单

无参数运行：

```text
=== 希沃安装/卸载工具 ===
1. 安装希沃
2. 卸载希沃
0. 退出程序
```

安装流程会继续显示本地版本列表：

```text
==================== 可选版本 ====================
[0] 下载新版本
[1] 1.2.3.4 (SeewoServiceSetup_1.2.3.4.exe)
...
==================================================
```

## 注意事项

- 下载文件小于 1MB 会被视为无效并删除；
- HTTP 404 表示版本不存在或已下架；
- 卸载时若提示“模板缺失”，检查 `HugoFakeVerify.exe`；
- 卸载时若提示“可能未安装希沃”，检查默认安装路径下的 `Uninstall.exe`；
- 卸载会覆盖 `uninstall_verify.exe`，原文件备份在 Seewo 安装目录的 `OLD*.tmp`，不会自动还原；

## 日志

写入程序目录下的 `HugoInstaller.log`，记录安装包扫描、下载、重命名、启动安装/卸载、文件替换与异常信息。

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。