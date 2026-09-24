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
#ifndef HUGO_WND_REC_UTILS_H
#define HUGO_WND_REC_UTILS_H

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <utility>

void Print(const std::wstring& s);
void PrintLine(const std::wstring& s = L"");
std::wstring Trim(const std::wstring& s);
std::wstring ReadLine();
void LogEvent(const std::wstring& event, const std::wstring& detail);
void OpenFile(const std::filesystem::path& path);
std::wstring GetProcessNameByPid(DWORD pid);

// 尺寸解析 / 存取
std::optional<std::pair<int, int>> ParseSize(const std::wstring& s);
bool SizeLess(const std::wstring& a, const std::wstring& b);
std::set<std::wstring> LoadSizeSet(const std::filesystem::path& path);
void SaveSizeSet(const std::filesystem::path& path, const std::set<std::wstring>& sizes);

#endif // HUGO_WND_REC_UTILS_H