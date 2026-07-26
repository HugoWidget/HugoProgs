# HugoFreezeDisk

## 项目简介

HugoFreezeDisk 是基于 C++/C 开发的 Windows 平台驱动级磁盘读写工具，驱动级删除特定文件来解除还原。

出于安全考虑，HugoFreezeDisk核心代码暂不开源，开发者不对该方法可用性与安全性提供保障。

由于项目使用漏驱加载`WinDisk_xxx.sys`，如果驱动加载失败，

可以手动用其他方法加载驱动后以管理员模式运行`HugoFrzDskMain.exe`。

## 输出示例

```txt
[0]Program started.
[1]Driver initialize successfully.
[2]Start HugoFrzDskMain.exe.
HugoFrzDskMain Output :
Successfully deleted
[2]HugoFrzDskMain.exe exited.
[3]Note: The driver will remain loaded until the next reboot.
[3]Program exits.
```

## 许可证

本项目采用 GNU General Public License v3.0 (GPLv3) 许可证开源，详见 LICENSE 文件。