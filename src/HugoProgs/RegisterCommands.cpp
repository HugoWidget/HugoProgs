/*
 * Copyright 2025-2026 howdy213, JYardX
 *
 * This file is part of HugoProgs.
 *
 * HugoProgs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HugoProgs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HugoProgs. If not, see <https://www.gnu.org/licenses/>.
 */
#include "WinUtils/WinPch.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>

#include "WinUtils/Console.h"
#include "WinUtils/WinUtils.h"
#include "HugoUtils/HArt.h"
#include "WinUtils/ConsoleMenu.h"
#include "HugoUtils/GPL3.h"
#include "ConsoleTable.h"
#include "WinUtils/WinReg.h"
#include "ExtensionMenu.h"

#include "HugoProgsCommon.h"
#include "RegisterCommands.h"

using namespace std;
using namespace WinUtils;
namespace fs = filesystem;

namespace
{
	fs::path GetGuideFlagPath()
	{
		return fs::path(GetCurrentProcessDir() + L"HugoProgs.guide.done");
	}

	bool AskYesNo(const wstring& prompt, bool defaultYes)
	{
		while (true)
		{
			wstring input = ReadUserInput(prompt);

			size_t start = input.find_first_not_of(L" \t\r\n");
			if (start == wstring::npos)
				return defaultYes;

			size_t end = input.find_last_not_of(L" \t\r\n");
			input = input.substr(start, end - start + 1);

			transform(input.begin(), input.end(), input.begin(), ::towlower);

			if (input == L"y" || input == L"yes" || input == L"是" || input == L"1")
				return true;

			if (input == L"n" || input == L"no" || input == L"否" || input == L"0")
				return false;

			wcout << L"请输入 y 或 n。\n";
		}
	}
}

bool IsGuideCompleted()
{
	return fs::exists(GetGuideFlagPath());
}

void MarkGuideCompleted()
{
	std::ofstream ofs(GetGuideFlagPath());
	if (ofs)
		ofs << "completed\n";
}

void RunGuide(ConsoleMenu& menu)
{
	wcout << L"\n==================== HugoProgs 使用引导 ====================\n";
	wcout << L"依赖关系：TaskManager 依赖 Launcher 启动；Launcher 依赖 AutoStartService 注册。\n";
	wcout << L"建议配置顺序：AutoStartService -> Launcher.ini -> tasks.ini。\n\n";

	if (AskYesNo(L"是否现在跳过引导？[y/N] ", false))
	{
		MarkGuideCompleted();
		wcout << L"已跳过引导，下次不再自动提示。可随时输入 guide 重新运行。\n";
		return;
	}

	// 1. 自启动服务
	if (AskYesNo(L"1/3 是否注册自启动服务 AutoStartService？需要管理员权限。[Y/n] ", true))
	{
		if (!IsCurrentProcessAdmin())
		{
			wcout << L"当前不是管理员，安装可能失败。可稍后执行 privilege 提权。\n";
		}
		menu.execute(L"config/service/install", true);
	}
	else
	{
		wcout << L"已跳过自启动服务注册。\n";
	}

	// 2. Launcher.ini
	if (AskYesNo(L"2/3 是否编辑 Launcher.ini？自启动后由 Launcher 依次启动程序。[Y/n] ", true))
	{
		menu.execute(L"config/auto", true);
	}
	else
	{
		wcout << L"已跳过 Launcher.ini 编辑。\n";
	}

	// 3. tasks.ini
	if (AskYesNo(L"3/3 是否编辑 tasks.ini？TaskManager 定时任务依赖 Launcher 已启动。[Y/n] ", true))
	{
		menu.execute(L"config/task", true);
	}
	else
	{
		wcout << L"已跳过 tasks.ini 编辑。\n";
	}

	MarkGuideCompleted();
	wcout << L"引导完成，下次不再自动提示。可随时输入 guide 重新运行。\n";
}

void MaybeRunGuide(ConsoleMenu& menu)
{
	if (IsGuideCompleted())
		return;

	if (AskYesNo(L"\n检测到尚未完成使用引导，是否立即开始？[Y/n] ", true))
	{
		RunGuide(menu);
	}
	else
	{
		MarkGuideCompleted();
		wcout << L"已跳过引导，下次不再自动提示。可输入 guide 重新运行。\n";
	}
	system("pause");
}

void registerObject(ConsoleMenu& menu)
{
	// 通用命令
	menu.addCommonCommand(L"guide", L"使用引导", [&menu](ConsoleMenu&, Args) {
		RunGuide(menu);
		});

	menu.addCommonCommand(L"exit", L"退出", [](ConsoleMenu&, Args) {
		wcout << L"\n程序已退出";
		exit(0);
		});

	menu.addCommonCommand(L"help", L"帮助", [&menu](ConsoleMenu&, Args) {
		menu.setDisplayMode(MenuDisplayMode::Exclusive);
		wcout << L"\n==================================== HugoProgs 帮助 ====================================\n";
		wcout << L"命令格式1：<command> [parameters]\n";
		wcout << L"输入命令后按回车执行，部分命令可能需要额外输入参数，程序会提示相关信息\n";
		wcout << L"命令格式2：c<number>/s<number> 适用于命令/子菜单的快捷数字输入\n";
		wcout << L"例如：输入c1执行当前菜单的第一个命令，输入s2进入当前菜单的第二个子菜单\n";
		wcout << L"输入..可以返回上一级菜单\n";
		wcout << L"输入common可以查看所有菜单通用命令\n";
		wcout << L"祝您使用愉快！\n";
		wcout << L"=======================================================================================\n";
		menu.setDisplayMode(MenuDisplayMode::Normal);
		});

	menu.addCommonCommand(L"license", L"许可", [](ConsoleMenu&, Args) {
		wcout <<
			L"HugoProgs  Copyright (C) 2026  HugoWidget\n"
			L"This program comes with ABSOLUTELY NO WARRANTY; for details type `w'.\n"
			L"This is free software, and you are welcome to redistribute it\n"
			L"under certain conditions; type `c' for details.\n\n";
		wchar_t res = _getwch();
		if (res == L'c') ShowLicense((GetCurrentProcessDir() + L"LICENSE").c_str());
		if (res == L'w') ShowWarranty();
		});

	menu.addCommonCommand(L"about", L"关于", [](ConsoleMenu&, Args) {
		wstring url = L"https://github.com/HugoWidget/HugoProgs";
		wcout << L"源代码仓库 " << url << L" 输入g以跳转\n";
		wchar_t ch = _getwch();
		if (ch == L'G' || ch == L'g') RunExternalProgram(url);
		});
	menu.addCommonCommand(L"privilege", L"提升至管理员权限", [](ConsoleMenu&, Args) {
		if (!IsCurrentProcessAdmin()) {
			RequireAdminPrivilege(true);
		}
		});

	// ==================== 子菜单：希沃虚拟磁盘管理器 ====================
	auto& mountMenu = menu.addSubmenu(L"mount", L"希沃虚拟磁盘管理器");
	{
		mountMenu.addCommand(L"list", L"枚举虚拟磁盘", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--list");
			});
		mountMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});

		mountMenu.addCommand(L"mnt", L"挂载 <disk> <part> [drive]", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			if (params.size() < 2) {
				wcout << L"用法: mnt <disk> <partition> [driveLetter]\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--mount " + params[0] + L" " + params[1];
			if (params.size() >= 3) {
				cmdLine += L" " + params[2];
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});

		mountMenu.addCommand(L"unmnt", L"卸载 <disk> <part>/<drive>", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			if (params.empty()) {
				wcout << L"用法: unmnt <driveLetter>  或  unmnt <disk> <partition>\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--unmount ";
			if (params.size() == 1 && iswalpha(params[0][0])) {
				cmdLine += wstring(1, towupper(params[0][0]));
			}
			else if (params.size() >= 2) {
				cmdLine += params[0] + L" " + params[1];
			}
			else {
				wcout << L"用法: unmnt <driveLetter>  或  unmnt <disk> <partition>\n";
				return;
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});

		mountMenu.addCommand(L"log", L"挂载日志盘 [drive]", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--log";
			if (!params.empty()) {
				cmdLine += L" " + params[0];
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});

		mountMenu.addCommand(L"conf", L"挂载配置盘 [drive]", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			wstring progPath = GetExternalProgramPath(L"HugoMount.exe");
			if (progPath.empty()) return;

			wstring cmdLine = L"--conf";
			if (!params.empty()) {
				cmdLine += L" " + params[0];
			}
			ExecuteProgramInCurrentConsole(progPath, cmdLine);
			});
		mountMenu.addCommand(L"browse.cli", L"在命令行中浏览", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"CExplorer.exe");
			ExecuteProgramInCurrentConsole(progPath);
			});
		mountMenu.addCommand(L"browse.gui", L"在PyExplorer中浏览", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"PyExplorer.exe");
			ExecuteProgramInCurrentConsole(progPath);
			});
	}

	// ==================== 子菜单：希沃冰点配置工具 ====================
	auto& freezeMenu = menu.addSubmenu(L"freeze", L"希沃冰点配置工具");
	{
		freezeMenu.addCommand(L"info", L"功能介绍", [](ConsoleMenu&, Args) {
			wcout << L"HugoWidget 提供多种方法管理还原，可按需取用\n";
			ConsoleTable table;
			table.addColumn(L"工具");
			table.addColumn(L"介绍");
			table.addColumn(L"特点");
			table.addRow(L"Api", L"与希沃冰点服务交互", L"安全性最高");
			table.addRow(L"Driver", L"与希沃冰点驱动交互", L"速度最快，功能最全");
			table.addRow(L"File", L"修改本地冰点文件", L"高度自定义");
			table.addRow(L"Hook", L"驱动运行时内存篡改", L"支持动态修改");
			table.addRow(L"", L" + 内核执行流劫持", L"");
			table.addRow(L"Disk", L"驱动级删除配置", L"通用性强");
			table.addRow(L"Offcial", L"官方配置修改工具", L"原生体验");
			table.addRow(L"WinPE", L"在WinPE中删除配置文件", L"通用性强");
			table.print();
			});
		// 驱动版本
		freezeMenu.addCommand(L"drv.get", L"查询驱动冻结状态", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--query");
			});
		freezeMenu.addCommand(L"drv.set", L"设置驱动冻结盘符", [](ConsoleMenu&, Args) {
			wstring input = ReadUserInput(L"请输入要冻结的盘符（如 CD 表示C和D盘），输入 0 解除所有冻结：");
			if (input.empty()) {
				wcout << L"输入无效\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--set " + input);
			});
		freezeMenu.addCommand(L"drv.run", L"运行HugoFreezeDriver.exe", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath);
			});
		freezeMenu.addCommand(L"drv.hlp", L"帮助", [](ConsoleMenu& menu, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDriver.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
		// API版本
		freezeMenu.addCommand(L"api.get", L"查询API冻结状态", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--get-disk");
			});
		freezeMenu.addCommand(L"api.set", L"设置API冻结磁盘", [](ConsoleMenu&, Args) {
			wstring vol = ReadUserInput(L"请输入磁盘标识（0代表解除全部冻结）：");
			if (vol.empty()) {
				wcout << L"输入无效\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--set-vol " + vol);
			});
		freezeMenu.addCommand(L"api.try", L"尝试API冻结磁盘", [](ConsoleMenu&, Args) {
			wstring disk = ReadUserInput(L"请输入磁盘标识（0代表解除全部冻结）：");
			if (disk.empty()) {
				wcout << L"输入无效\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--try-protect " + disk);
			});
		freezeMenu.addCommand(L"api.hlp", L"帮助", [](ConsoleMenu& menu, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeApi.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
		freezeMenu.addCommand(L"disk", L"运行HugoFreezeDisk.exe", [](ConsoleMenu& menu, Args) {
			if (!IsCurrentProcessAdmin()) {
				wcout << L"注意：HugoFreezeDisk.exe 需要管理员权限才能正常运行\n";
			}
			wcout << L"警告：本程序仅为概念验证（PoC）用途，\n"
				"开发者未对其可用性、稳定性和安全性做出任何明示或暗示的保证。\n"
				"使用者继续使用本程序即表示同意自行承担所有风险，\n"
				"因使用本程序所引发的任何安全问题、数据丢失、系统损害或其他法律责任，\n"
				"开发者均不承担任何责任。" << endl;
			wcout << L"继续使用？[Y/N]\n";
			if (towupper(_getwch()) != L'Y') {
				wcout << L"\n已取消操作\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoFreezeDisk/HugoFreezeDisk.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"");
			wcout << L"如果程序正常运行，冰点还原将会在重启后解除。\n";
			});
		auto& hookMenu = freezeMenu.addSubmenu(L"hook", L"运行HugoFrzDrvHook.exe");
		{
			hookMenu.addCommand(L"about-hook", L"关于HugoFrzDrvHook.exe", [](ConsoleMenu& menu, Args) {
				if (!IsCurrentProcessAdmin()) {
					wcout << L"注意：HugoFrzDrvHook.exe 需要管理员权限才能正常运行\n";
				}
				wcout << L"警告：本程序将对希沃驱动进行修改。\n"
					"开发者未对其可用性、稳定性和安全性做出任何明示或暗示的保证。\n"
					"使用者继续使用本程序即表示同意自行承担所有风险，\n"
					"因使用本程序所引发的任何安全问题、数据丢失、系统损害或其他法律责任，\n"
					"开发者均不承担任何责任。" << endl;
				wcout << L"项目开源于 https://github.com/HugoWidget/HugoFrzDrvHook\n";
				});

			// 封装检查管理员权限并执行外部程序
			auto execHook = [](const wstring& args) {
				if (!IsCurrentProcessAdmin()) {
					wcout << L"此功能需要管理员权限。请以管理员身份运行本程序。\n";
					return;
				}
				wstring progPath = GetExternalProgramPath(L"HugoFrzDrvHook.exe");
				if (!progPath.empty())
					ExecuteProgramInCurrentConsole(progPath, args);
				};

			// cfg – 修改冰点还原配置
			hookMenu.addCommand(L"cfg", L"修改冰点还原配置 (保护卷)", [execHook](ConsoleMenu&, Args) {
				wstring drives = ReadUserInput(L"请输入要保护的盘符（如 C D，留空则取消所有保护）：");
				wstring args = L"cfg";
				if (!drives.empty()) args += L" " + drives;
				execHook(args);
				});

			// mjfunc – 设置磁盘驱动分发例程
			hookMenu.addCommand(L"mjfunc", L"设置磁盘驱动分发例程 (0/1)", [execHook](ConsoleMenu&, Args) {
				wstring val = ReadUserInput(L"输入 0(使用SWFreeze挂钩) 或 1(恢复原生disk.sys)：");
				if (val != L"0" && val != L"1") {
					wcout << L"无效输入。\n";
					return;
				}
				execHook(L"mjfunc " + val);
				});

			// volume – 临时修改卷保护状态
			hookMenu.addCommand(L"volume", L"临时修改卷保护状态", [execHook](ConsoleMenu&, Args) {
				wstring drives = ReadUserInput(L"请输入盘符（空格分隔，如 C D）：");
				if (drives.empty()) {
					wcout << L"盘符不能为空。\n";
					return;
				}
				wstring status = ReadUserInput(L"保护状态 (0-禁用保护, 1-开启保护)：");
				if (status != L"0" && status != L"1") {
					wcout << L"无效状态。\n";
					return;
				}
				execHook(L"volume " + drives + L" " + status);
				});

			// 子菜单：whitelist 白扇区位图控制
			auto& whitelistMenu = hookMenu.addSubmenu(L"whitelist", L"白扇区位图控制");
			{
				whitelistMenu.addCommand(L"file", L"按文件设置白扇区位图", [execHook](ConsoleMenu&, Args) {
					wstring filepath = ReadUserInput(L"请输入文件完整路径：");
					if (filepath.empty()) {
						wcout << L"路径无效。\n";
						return;
					}
					wstring val = ReadUserInput(L"设置状态 (0-扇区重定向, 1-直接读写)：");
					if (val != L"0" && val != L"1") {
						wcout << L"无效输入。\n";
						return;
					}
					execHook(L"whitelist file \"" + filepath + L"\" " + val);
					});

				whitelistMenu.addCommand(L"sec", L"按扇区位图设值", [execHook](ConsoleMenu&, Args) {
					wstring drive = ReadUserInput(L"请输入盘符（如 C）：");
					wstring start = ReadUserInput(L"起始扇区（8的倍数）：");
					wstring length = ReadUserInput(L"长度（8的倍数）：");
					wstring val = ReadUserInput(L"值 (0或1)：");
					if (val != L"0" && val != L"1" || drive.empty() || start.empty() || length.empty()) {
						wcout << L"无效值。\n";
						return;
					}
					execHook(L"whitelist sec " + drive + L" " + start + L" " + length + L" " + val);
					});

				whitelistMenu.addCommand(L"in", L"导入白扇区位图", [execHook](ConsoleMenu&, Args) {
					wstring drive = ReadUserInput(L"盘符：");
					wstring start = ReadUserInput(L"起始扇区（8的倍数）：");
					wstring length = ReadUserInput(L"长度（8的倍数）：");
					wstring filepath = ReadUserInput(L"导入文件路径：");
					if (filepath.empty() || drive.empty() || start.empty() || length.empty()) {
						wcout << L"无效输入。\n";
						return;
					}
					execHook(L"whitelist in " + drive + L" " + start + L" " + length + L" \"" + filepath + L"\"");
					});

				whitelistMenu.addCommand(L"out", L"导出白扇区位图", [execHook](ConsoleMenu&, Args) {
					wstring drive = ReadUserInput(L"盘符：");
					wstring start = ReadUserInput(L"起始扇区（8的倍数）：");
					wstring length = ReadUserInput(L"长度（8的倍数）：");
					wstring filepath = ReadUserInput(L"导出文件路径：");
					if (filepath.empty() || drive.empty() || start.empty() || length.empty()) {
						wcout << L"无效输入。\n";
						return;
					}
					execHook(L"whitelist out " + drive + L" " + start + L" " + length + L" \"" + filepath + L"\"");
					});
			}

			// flt – 模拟冰点还原状态
			hookMenu.addCommand(L"flt", L"模拟冰点还原状态", [execHook](ConsoleMenu&, Args) {
				wstring input = ReadUserInput(L"输入盘符（如 C D）或输入 off 禁用此功能：");
				if (input.empty()) {
					wcout << L"输入无效。\n";
					return;
				}
				execHook(L"flt " + input);
				});

			// info – 获取冰点还原相关信息
			hookMenu.addCommand(L"info", L"获取冰点还原相关信息", [execHook](ConsoleMenu&, Args) {
				execHook(L"info");
				});

			// help – 显示帮助
			hookMenu.addCommand(L"hlp", L"显示 HugoFrzDrvHook 帮助", [execHook](ConsoleMenu&, Args) {
				execHook(L"help");
				});
		}
		// 文件版本
		freezeMenu.addCommand(L"file", L"启动HugoFreezeFile.exe", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoFreezeFile.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"");
			});
		freezeMenu.addCommand(L"off", L"希沃官方工具（可能需要先关闭SeewoCore）", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"SeewoFreeze\\SeewoFreezeUI.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"", false);
			});
		freezeMenu.addCommand(L"stop", L"关闭希沃进程", [](ConsoleMenu& menu, Args) {
			menu.execute(L"launch/stop", false);
			});
	}

	// ==================== 子菜单：希沃服务与网络控制 ====================
	auto& basicMenu = menu.addSubmenu(L"basic", L"希沃服务与网络控制");
	{
		basicMenu.addCommand(L"info", L"查询基本信息", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInfo.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath);
			});
		basicMenu.addCommand(L"off", L"关闭文件保护", [](ConsoleMenu& menu, Args) {
			menu.execute(L"fprotect/disable", false);
			});
		basicMenu.addCommand(L"disable", L"禁用希沃服务", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--disable");
			});
		basicMenu.addCommand(L"enable", L"启用希沃服务", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--enable");
			});
		basicMenu.addCommand(L"update.off", L"禁用希沃更新", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--disable-update");
			});
		basicMenu.addCommand(L"update.on", L"启用希沃更新", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--enable-update");
			});
		basicMenu.addCommand(L"net.off", L"禁用希沃网络", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--disable-net");
			});
		basicMenu.addCommand(L"net.on", L"启用希沃网络", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--enable-net");
			});
		basicMenu.addCommand(L"clear", L"清除所有防火墙规则", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--clear-rules");
			});
		basicMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoDisable.exe");
			if (!progPath.empty()) ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：DLL注入工具 ====================
	auto& injectMenu = menu.addSubmenu(L"inject", L"DLL注入工具");
	{
		injectMenu.addCommand(L"in", L"进程监控+注入", [](ConsoleMenu&, Args) {
			wcout << L"\nDLL注入配置\n";
			wstring dllPath = ReadUserInput(L"请输入DLL文件完整路径：");
			if (dllPath.empty() || !fs::exists(dllPath)) {
				wcout << L"错误：DLL路径无效\n";
				return;
			}
			wstring procName = ReadUserInput(L"请输入目标进程名：");
			if (procName.empty()) {
				wcout << L"错误：进程名不能为空\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--inject " + dllPath + L" " + procName, false);
			});

		injectMenu.addCommand(L"un", L"卸载DLL", [](ConsoleMenu&, Args) {
			wcout << L"\nDLL卸载配置\n";
			wstring dllPath = ReadUserInput(L"请输入DLL文件完整路径：");
			if (dllPath.empty() || !fs::exists(dllPath)) {
				wcout << L"错误：DLL路径无效\n";
				return;
			}
			wstring procName = ReadUserInput(L"请输入目标进程名：");
			if (procName.empty()) {
				wcout << L"错误：进程名不能为空\n";
				return;
			}
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--uninject " + dllPath + L" " + procName);
			});

		injectMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInjector.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：希沃启动工具 ====================
	auto& launchMenu = menu.addSubmenu(L"launch", L"希沃启动工具");
	{
		launchMenu.addCommand(L"start", L"启动希沃", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--start");
			});

		launchMenu.addCommand(L"stop", L"终止希沃进程", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--stop", false);
			});

		launchMenu.addCommand(L"kill", L"单次终止希沃进程", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--kill", false);
			});

		launchMenu.addCommand(L"inst", L"安装希沃管家", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--install");
			});

		launchMenu.addCommand(L"uninst", L"卸载希沃管家", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--uninstall");
			});
		launchMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring launchPath = GetExternalProgramPath(L"HugoLaunchTool.exe");
			if (!launchPath.empty())
				ExecuteProgramInCurrentConsole(launchPath, L"--help");
			wstring installPath = GetExternalProgramPath(L"HugoInstaller.exe");
			if (!installPath.empty())
				ExecuteProgramInCurrentConsole(installPath, L"--help");
			});
	}

	// ==================== 子菜单：希沃文件保护工具 ====================
	auto& fprotectMenu = menu.addSubmenu(L"fprotect", L"希沃文件保护工具");
	{
		fprotectMenu.addCommand(L"enable", L"启用文件保护", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--enable");
			});
		fprotectMenu.addCommand(L"disable", L"禁用文件保护", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--disable");
			});
		fprotectMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoProtect.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：希沃锁屏工具 ====================
	auto& lockMenu = menu.addSubmenu(L"lock", L"希沃锁屏工具");
	{
		lockMenu.addCommand(L"info", L"说明", [=](ConsoleMenu&, Args) {
			wcout << L"这些工具不适合在此调用，可使用execute命令来执行，\n"
				<< L"使用方式详见帮助文档。";
			});
		lockMenu.addCommand(L"hlp", L"HugoLock 帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLock.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
		lockMenu.addCommand(L"assist.hlp", L"HugoLockAssistant 帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoLockAssistant.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：希沃密码工具 ====================
	auto& pswMenu = menu.addSubmenu(L"psw", L"希沃密码工具");
	{
		pswMenu.addCommand(L"manual", L"手动", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoPassword.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--manual");
			});
		pswMenu.addCommand(L"auto", L"自动", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoPassword.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--auto");
			});
		pswMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoPassword.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
		pswMenu.addCommand(L"break", L"修改dll以去除密码", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoBreak.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath);
			});
		pswMenu.addCommand(L"break.hlp", L"修改dll工具帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoBreak.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// ==================== 子菜单：WinPE 工具 ====================
	auto& winpeMenu = menu.addSubmenu(L"winpe", L"WinPE工具");
	{
		winpeMenu.addCommand(L"launch", L"打开 WinPE 部署工具", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoWinPE/PEOutside.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--launch");
			});

		winpeMenu.addCommand(L"run", L"启动 WinPE 部署工具", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoWinPE/PEOutside.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath);
			});
	}

	menu.addCommandAtPath(L"", L"common", L"查看通用命令", [](ConsoleMenu&, Args) {});

	// ==================== 子菜单：.hps 文件绑定 ====================
	auto& hpsMenu = menu.addSubmenu(L"hps", L".hps 脚本文件绑定");
	{

		// 命令：关联 .hps 文件
		hpsMenu.addCommand(L"assoc", L"将 .hps 文件关联到 HugoProgs",
			[](ConsoleMenu&, Args) {
				// 检查是否以管理员身份运行
				if (!IsCurrentProcessAdmin()) {
					wcout << L"错误：关联操作需要管理员权限。\n";
					return;
				}

				wstring exePath = GetCurrentProcessPath();
				try {
					WinUtils::RegKey keyExt;
					keyExt.Create(HKEY_CLASSES_ROOT, L".hps", KEY_WRITE);
					keyExt.SetStringValue(L"", L"HugoProgs.Script");

					WinUtils::RegKey keyMain;
					keyMain.Create(HKEY_CLASSES_ROOT, L"HugoProgs.Script", KEY_WRITE);

					WinUtils::RegKey keyIcon;
					keyIcon.Create(keyMain.Get(), L"DefaultIcon", KEY_WRITE);
					wstring iconPath = L"\"" + exePath + L"\",0";
					keyIcon.SetStringValue(L"", iconPath);

					WinUtils::RegKey keyCommand;
					keyCommand.Create(keyMain.Get(), L"shell\\open\\command", KEY_WRITE);
					wstring cmdLine = L"\"" + exePath + L"\" \"%1\"";
					keyCommand.SetStringValue(L"", cmdLine);

					wcout << L".hps 文件已成功关联到 HugoProgs。\n";
				}
				catch (const WinUtils::RegException& e) {
					wcout << L"关联失败：注册表操作错误，代码 " << e.code().value()
						<< L"，消息：" << e.what() << L"\n";
				}
			});

		// 命令：取消 .hps 文件关联
		hpsMenu.addCommand(L"disassoc", L"取消 .hps 文件关联",
			[](ConsoleMenu&, Args) {
				if (!IsCurrentProcessAdmin()) {
					wcout << L"错误：取消关联需要管理员权限。\n";
					return;
				}

				try {
					WinUtils::RegKey keyExt;
					if (keyExt.TryOpen(HKEY_CLASSES_ROOT, L".hps", DELETE) ||
						keyExt.TryOpen(HKEY_CLASSES_ROOT, L".hps", KEY_WRITE)) {
						LSTATUS status = RegDeleteTreeW(keyExt.Get(), nullptr);
						if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
							throw WinUtils::RegException(status, "RegDeleteTree failed for .hps");
					}

					WinUtils::RegKey keyMain;
					if (keyMain.TryOpen(HKEY_CLASSES_ROOT, L"HugoProgs.Script", DELETE) ||
						keyMain.TryOpen(HKEY_CLASSES_ROOT, L"HugoProgs.Script", KEY_WRITE)) {
						LSTATUS status = RegDeleteTreeW(keyMain.Get(), nullptr);
						if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
							throw WinUtils::RegException(status, "RegDeleteTree failed for HugoProgs.Script");
					}

					wcout << L".hps 文件关联已移除。\n";
				}
				catch (const WinUtils::RegException& e) {
					wcout << L"取消关联失败：注册表操作错误，代码 " << e.code().value()
						<< L"，消息：" << e.what() << L"\n";
				}
			});

		hpsMenu.addCommand(L"run", L"执行脚本文件 <path>", [&menu](ConsoleMenu&, Args args) {
			if (auto path = args.getParam(L"", 0)) {
				wstring scriptPath = ResolvePath(*path);
				if (!IsScriptFile(scriptPath)) {
					wcout << L"错误：不是有效的 .hps 文件或文件不存在。\n";
					return;
				}
				ExecuteScriptFile(scriptPath, &menu);
			}
			else wcout << L"用法: run <脚本文件路径>\n";
			});
	}

	// ==================== 子菜单：执行外部程序 ====================
	auto& execMenu = menu.addSubmenu(L"execute", L"执行外部程序");
	{
		auto join = [](vector<wstring> params) {
			wstring cmdLine;
			for (size_t i = 1; i < params.size(); ++i) {
				if (i > 1) cmdLine += L' ';
				cmdLine += params[i];
			}
			return cmdLine;
			};
		auto exec = [&](const wstring& verb, const Args& args) {
			auto params = args.getParams(L"");
			if (params.empty()) {
				wcout << L"用法: " << verb << L" <路径> [参数...]\n";
				return;
			}
			HINSTANCE ret = WinUtils::RunExternalProgram(ResolvePath(params[0]), verb, join(params));
			wcout << ((INT_PTR)ret <= 32 ? L"执行失败" : L"程序已启动") << L"\n";
			};

		execMenu.addCommand(L"open", L"普通权限打开 <路径> [参数...]",
			[exec](ConsoleMenu&, Args a) { exec(L"open", a); });
		execMenu.addCommand(L"runas", L"管理员权限运行 <路径> [参数...]",
			[exec](ConsoleMenu&, Args a) { exec(L"runas", a); });
		execMenu.addCommand(L"create", L"CreateProcess阻塞打开 <路径> [参数...]",
			[exec, join](ConsoleMenu&, Args a) {
				auto params = a.getParams(L"");
				if (!params.empty()) {
					ExecuteProgramInCurrentConsole(params[0], join(params));
				}
			});
	}

	auto& configMenu = menu.addSubmenu(L"config", L"配置与自启动");
	{
		configMenu.addCommand(L"auto", L"编辑配置文件", [](ConsoleMenu&, Args) {
			RunExternalProgram(GetCurrentProcessDir() + L"Launcher.ini");
			});
		configMenu.addCommand(L"task", L"编辑任务配置", [](ConsoleMenu&, Args) {
			RunExternalProgram(GetCurrentProcessDir() + L"tasks.ini");
			});
		configMenu.addCommand(L"ext", L"编辑扩展配置", [](ConsoleMenu&, Args) {
			RunExternalProgram(GetCurrentProcessDir() + L"extension.ini");
			});
		auto& serviceMenu = configMenu.addSubmenu(L"service", L"服务管理");
		{
			serviceMenu.addCommand(L"install", L"安装自启动服务", [](ConsoleMenu&, Args args) {
				wstring progPath = GetExternalProgramPath(L"AutoStartService.exe");
				if (!progPath.empty())
					ExecuteProgramInCurrentConsole(progPath, L"/install");
				});
			serviceMenu.addCommand(L"uninstall", L"卸载自启动服务", [](ConsoleMenu&, Args args) {
				wstring progPath = GetExternalProgramPath(L"AutoStartService.exe");
				if (!progPath.empty())
					ExecuteProgramInCurrentConsole(progPath, L"/uninstall");
				});
			serviceMenu.addCommand(L"query", L"查询服务状态", [](ConsoleMenu&, Args args) {
				wstring progPath = GetExternalProgramPath(L"AutoStartService.exe");
				if (!progPath.empty()) {
					DWORD exitCode = 0;
					wcout << L"服务路径: ";
					ExecuteProgramInCurrentConsole(progPath, L"/query", true, &exitCode);
					wcout << L"服务状态: " << (exitCode ? L"已注册" : L"未注册") << L"\n";
				}
				});
		}
	}

	auto& screensaverMenu = menu.addSubmenu(L"screensaver", L"屏幕保护程序");
	{
		screensaverMenu.addCommand(L"on", L"打开屏幕保护程序", [](ConsoleMenu&, Args args) {
			wcout << L"功能未完成";
			});
		screensaverMenu.addCommand(L"off", L"关闭屏幕保护程序", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"HugoScreenSaver.exe");
			ExecuteProgramInCurrentConsole(progPath);
			});
		screensaverMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"HugoScreenSaver.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	auto& wndrecMenu = menu.addSubmenu(L"wndrec", L"窗口记录与拦截");
	{
		wndrecMenu.addCommand(L"run", L"运行", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"HugoWndRec.exe");
			ExecuteProgramInCurrentConsole(progPath, L"", false);
			});
	}

	// ==================== 子菜单：内存扫描 ====================
	auto& scanMenu = menu.addSubmenu(L"scan", L"内存扫描工具");
	{
		scanMenu.addCommand(L"extract", L"提取希沃密码", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoScan.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--extract");
			});
		scanMenu.addCommand(L"mem", L"内存特征码扫描 <进程名|PID> <特征码>", [](ConsoleMenu&, Args args) {
			auto params = args.getParams(L"");
			if (params.size() < 2) {
				wcout << L"用法: mem <进程名|PID> <特征码>  例: mem SeewoCore.exe \"48 8B ?? 89 05\"\n";
				return;
			}
			wstring pattern = params[1];
			for (size_t i = 2; i < params.size(); ++i) pattern += L' ' + params[i];
			wstring progPath = GetExternalProgramPath(L"HugoScan.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--scan " + params[0] + L" \"" + pattern + L"\"");
			});
		scanMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args) {
			wstring progPath = GetExternalProgramPath(L"HugoScan.exe");
			if (!progPath.empty())
				ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	auto& logsMenu = menu.addSubmenu(L"logs", L"日志");
	{
		logsMenu.addCommand(L"clr", L"清除所有日志", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"HugoLogs.exe");
			ExecuteProgramInCurrentConsole(progPath, L"--delete");
			});
		logsMenu.addCommand(L"ls", L"列出所有日志", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"HugoLogs.exe");
			ExecuteProgramInCurrentConsole(progPath, L"--list");
			});
		logsMenu.addCommand(L"run", L"打开 HugoLogs", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"HugoLogs.exe");
			ExecuteProgramInCurrentConsole(progPath);
			});
		logsMenu.addCommand(L"hlp", L"帮助", [](ConsoleMenu&, Args args) {
			wstring progPath = GetExternalProgramPath(L"HugoLogs.exe");
			ExecuteProgramInCurrentConsole(progPath, L"--help");
			});
	}

	// 从 extension.ini 加载自定义菜单项
	LoadExtensionMenu(menu);
}