# HugoProgs

## 项目介绍

帮助广大电教委对希沃功能进行增强与补充

## 环境依赖

Visual Studio 2022

## 功能说明

| 程序文件                               | 核心功能                                                     |
| -------------------------------------- | ------------------------------------------------------------ |
| **[HugoBreak](docs/HugoBreak.md)** | 修改`bind_zmodule.dll`以解除密码 |
| **[HugoDisable](docs/HugoDisable.md)** | 禁用 `SeewoServiceAssistant.exe`或阻止其通过防火墙；禁用希沃更新 |
| **[HugoFakeVerify](docs/HugoFakeVerify.md)** | 一个空程序。                                                 |
| **[HugoFreezeApi](docs/HugoFreezeApi.md)** | 通过本地服务 API 与希沃冰点服务交互，可设置冻结磁盘、查询冻结状态、尝试保护操作。 |
| **[HugoFreezeDriver](docs/HugoFreezeDriver.md)** | 驱动级冰点控制工具，可查询驱动状态、运行时配置，并冻结/解冻指定盘符。 |
| **[HugoInjector](docs/HugoInjector.md)** | 通用 DLL 注入器，结合 `HugoHSSA.dll` 解除锁屏与热键限制。 |
| **[HugoInstaller](docs/HugoInstaller.md)** | 希沃安装/卸载管理器：可下载指定版本或最新版。                |
| **[HugoLaunchTool](docs/HugoLaunchTool.md)** | 控制希沃核心进程的启动与终止。                               |
| **[HugoLock](docs/HugoLock.md)** | 实时隐藏“希沃管家”锁屏窗口，强制解除锁屏，也可以使用其解除屏保（HugoDbg也支持） |
| **[HugoLockAssistant](docs/HugoLockAssistant.md)** | 调用不同方式实现锁屏/屏保解除。                                |
| **[HugoMount](docs/HugoMount.md)** | 虚拟磁盘挂载工具，可列出、挂载、卸载希沃的日志盘、配置盘等。 |                                                      
| **[HugoProgs](docs/HugoProgs.md)** | 主菜单程序，集成所有工具，提供交互界面，支持脚本执行。           |
| **[HugoProtect](docs/HugoProtect.md)** | 开关希沃的文件保护功能。                                     |
| **[HugoPassword](docs/HugoPassword.md)** | 枚举以找回希沃管家/锁屏密码。                                |
## 编译运行

1. 执行`git clone https://github.com/HugoWidget/HugoProgs --recursive`
2. 打开HugoProgs.slnx
3. 如果使用lib（放在生成目录下）链接而不重新生成，请将HugoDeps中的附加依赖项HugoUtils去掉
4. 生成

## 说明

Release中HugoProgs.zip为该项目完整编译产物，前往[HugoSetup](https://github.com/HugoWidget/HugoSetup)获取已配置版本

## 快速入门

- 前往HugoSetup获取`HugoProgs-vx.x.x-bundle.zip`

- 双击`Start.exe`或管理员运行`HugoProgs.exe`

- 管理员权限下前往 config/service/install 注册自启动服务（实质是启动`Launcher.exe`而非`HugoProgs.exe`）

- 在 config/auto 修改启动配置（也就是`Launcher.ini`），格式为：

```ini
[节名，任意的英文字符串]
Program=.\HugoLockAssistant.exe
Params=--method=lock --mode=assist
RunAsAdmin=true
ShowWnd=0

[不与上面重复的节名]
Program=.\HotspotHelper.exe
Params=-start
RunAsAdmin=true

...

```

结合使用介绍修改Params来达到不同的功能，如果想要界面化的方式，见HugoWidgets，但可惜的是由于开发者时间精力有限，其没有自启动与锁屏相关功能，如果想做出贡献，欢迎加入。

你可能注意到task配置选项，且默认Launcher.ini配置中有.\TaskManager，其[配置方法](https://github.com/howdy213/WinTools)类似于Launcher，若不需要可以去掉

- 之后每次开机，将自动启动`Launcher.exe`并按照配置逐个打开目标程序

## 脚本执行

在 hps菜单 中有 `.hps` 脚本的相关功能，需要先使用`assoc`注册

语法如下：

[.hps文件语法](docs/hps.md)

## 项目依赖

[HugoUtils](https://github.com/HugoWidget/HugoUtils)(Hugo系列核心库)

## 许可证

本项目采用 GPLv3 许可证，详情参见 [LICENSE](LICENSE) 文件。

HugoUtils: [LGPLv3 许可证](licenses/LICENSE.LESSER-HugoUtils)

WinUtils:  [MIT 许可证](licenses/LICENSE-WinUtils)

hash-library: [zlib 许可证](licenses/LICENSE-hash-library)

swhelper：[MIT 许可证](licenses/LICENSE-swhelper)

cpp-httplib: [MIT 许可证](licenses/LICENSE-cpp-httplib)

mINI: [MIT 许可证](licenses/LICENSE-mINI)

WinReg: [MIT 许可证](licenses/LICENSE-WinReg)

## 致谢

[seewo_jailbreak](https://github.com/CatMe0w/seewo_jailbreak)：HugoBreak 方案来源

[swhelper](https://sw.s3.fan/swhelper/introduce.html)：部分项目（HugoMount，HugoFreezeDriver等）方案来源

[bruteforce_passwordv3](https://github.com/CatMe0w/bruteforce_passwordv3)：Password V3 算法

## 免责声明

本项目仅用于研究或教育目的，请勿将本项目用于可能违反当地法律、侵犯著作权或其他软件 EULA 的用途。若将本项目用于非法用途，一切后果由使用者承担，开发者不承担此类行为带来的任何后果或责任。
