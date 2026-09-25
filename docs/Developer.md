# HugoProgs 开发者文档

> 面向开发者：架构、构建、菜单注册机制、新增子工具的完整流程。
>
> 使用者请阅读 [HugoProgs 用户文档](./User.md)。
>

## 目录

- [1. 技术栈与总体架构](#1-技术栈与总体架构)
- [2. 仓库结构](#2-仓库结构)
- [3. 构建](#3-构建)
- [4. 菜单与命令设计约定](#4-菜单与命令设计约定)
- [5. 新增子工具](#5-新增子工具)

## 1. 技术栈与总体架构

| 项         | 内容                                                 |
| ---------- | ---------------------------------------------------- |
| 语言标准   | C++23                                                |
| IDE / 构建 | Visual Studio 2022，解决方案文件 `HugoProgs.slnx`    |
| 目标平台   | x64 / x86（解决方案同时提供两套配置）                |
| 核心库     | [HugoUtils](https://github.com/HugoWidget/HugoUtils) |

### 进程模型

HugoProgs 采用**「主程序 + 外部子工具进程」**的架构，而不是把所有功能编译进单一可执行文件：

```text
HugoProgs.exe（控制台主菜单）
├── 注册菜单树（registerObject）
├── 解析输入 / 解析 .hps 脚本
└── 按命令调用同目录下的子工具进程
```

这样设计的好处：

- **子工具可独立使用**：每个子工具都是完整可执行文件，可脱离主菜单单独运行。
- **主程序保持轻量**：主程序只负责菜单、参数校验与进程调度，不链接各子工具的业务代码。
- **发布即目录**：所有 exe 放在同一目录即可工作，无需注册表或安装程序。

代价是主程序必须校验子工具是否存在，并且新增功能时需要同时更新「子工具工程 + 菜单注册 + 发布包」。

## 2. 仓库结构

```text
HugoProgs/
├── HugoProgs.slnx              # 解决方案
├── README.md                   # 项目说明
├── docs/                       # 文档
├── icons/                      # 图标
├── licenses/                   # 第三方许可证副本
├── deps/
│   └── HugoUtils/              # 子模块：核心库（含 WinUtils）
└── src/
    ├── CommonProps.props       # 全项目公共编译/链接属性
    ├── HugoDeps/               # 空的“依赖聚合”工程（见下）
    ├── HugoProgs/              # 主菜单程序
    └── ...                     # 每个目录一个子工具工程
```

`src/HugoDeps` 是一个**刻意保持为空实现的工程**：它的唯一作用是作为依赖聚合点——`HugoProgs.slnx` 中每个子工具工程都声明 `BuildDependency` 指向 `src/HugoDeps/HugoDeps.vcxproj`，而 `HugoDeps` 再依赖 `deps/HugoUtils/HugoUtils.vcxproj`，从而保证「先构建 HugoUtils，再构建所有子工具」的顺序。

**使用预编译库时**：如果你使用放在生成目录下的 `lib` 进行链接而不重新生成 HugoUtils，请将 `HugoDeps` 中的附加依赖项 `HugoUtils` 去掉。

## 3. 构建

### 3.1 环境要求

- Visual Studio 2022（含 C++ 桌面开发工作负载）
- Windows SDK 10
- Git（需要拉取子模块）

### 3.2 构建步骤

```bash
# 1) 递归克隆
git clone https://github.com/HugoWidget/HugoProgs --recursive
```

2. 用 VS2022 打开 `HugoProgs.slnx`。
3. 选择配置，生成解决方案。

### 3.3 `CommonProps.props`

CommonProps.props 由各子工具工程引入，统一了以下属性：

| 配置项       | 值                                                           |
| ------------ | ------------------------------------------------------------ |
| 额外库目录   | `$(SolutionDir)$(Platform)$(Configuration)`、`$(SolutionDir)$(Configuration)` |
| 附加依赖项   | `$(CoreLibraryDependencies)`、`HugoUtils.lib`                |
| 额外包含目录 | `$(SolutionDir)deps/HugoUtils/include`                       |

新增子工具工程时，**记得引入该属性表**，否则会链接不到 `HugoUtils.lib` 或找不到头文件。

### 3.4 产物与目录

- 发布时需把主程序与**所有子工具 exe** 收集到同一目录；本仓库 Release 中的 `HugoProgs.zip` 即完整编译产物。

## 4. 菜单与命令设计约定

现有代码已形成一套清晰惯例，新增命令时请保持一致：

1. **子菜单命名**：全小写、简短、与功能对应（`mount`、`freeze`、`basic`、`fprotect`、`psw`）。
2. **命令命名**：短名优先，可含 `.` 分段（`drv.get`、`api.set`、`update.off`）。
3. **每个子菜单提供 `hlp`**：转发子工具的 `--help`。
4. **参数校验前置**：参数不足直接打印 `用法: ...`，不调用子工具。
6. **复用已有命令**：如 `basic/off` 直接 `menu.execute(L"fprotect/disable", false)`、`freeze/stop` 直接 `menu.execute(L"launch/stop", false)`，避免重复实现。

## 5. 新增子工具

假设要新增子工具 `HugoXxx`：

1. **建立工程**：在 `src/HugoXxx/` 下创建项目；工程需从`属性管理器`引入 CommonProps.props。

2. **加入解决方案**：在 HugoProgs.slnx 增加项目并添加依赖项 `HugoDeps`

3. **实现子工具**：自行解析命令行参数；需与主程序协作时保持参数风格一致。

4. **注册菜单**：在 HugoProgs.cpp 的 `registerObject` 中按现有写法添加子菜单或命令，典型实现为：

   ```cpp
   auto& xxxMenu = menu.addSubmenu(L"xxx", L"希沃XXX工具");
   {
       xxxMenu.addCommand(L"do", L"执行XXX", [](ConsoleMenu&, Args args) {
           auto params = args.getParams(L"");
           if (params.empty()) { wcout << L"用法: do <参数>\n"; return; }
           wstring progPath = GetExternalProgramPath(L"HugoXxx.exe");
           if (!progPath.empty())
               ExecuteProgramInCurrentConsole(progPath, L"--do " + params[0]);
       });
       xxxMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
           wstring p = GetExternalProgramPath(L"HugoXxx.exe");
           if (!p.empty()) ExecuteProgramInCurrentConsole(p, L"--help");
       });
   }
   ```

## 相关文档

- [HugoProgs 用户文档](./User.md)
- [`.hps` 脚本语法](./hps.md)
- [HugoUtils 核心库](https://github.com/HugoWidget/HugoUtils)
- [HugoSetup 安装包](https://github.com/HugoWidget/HugoSetup)

## 免责声明

本项目仅用于研究或教育目的，请勿将本项目用于可能违反当地法律、侵犯著作权或其他软件 EULA 的用途。若将本项目用于非法用途，一切后果由使用者承担，开发者不承担此类行为带来的任何后果或责任。