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
#pragma once
#include "WinUtils/WinPch.h"

#include "WinUtils/ConsoleMenu.h"
#include <string>
#include <Windows.h>

std::wstring GetExternalProgramPath(const std::wstring& programName);
std::wstring ReadUserInput(const std::wstring& prompt);

bool ExecuteProgramInCurrentConsole(
	const std::wstring& programPath,
	const std::wstring& args = L"",
	bool wait = true,
	DWORD* exitCode = nullptr);

bool IsScriptFile(const std::wstring& path);
int ExecuteScriptFile(const std::wstring& scriptPath, WinUtils::ConsoleMenu* menu);