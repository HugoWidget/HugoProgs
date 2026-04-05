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
#include <windows.h>

void SetInterceptEnabled(bool enable);
bool GetInterceptEnabled();
void UnlockOnce();
INT_PTR CALLBACK SimpleDialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK MainDialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam);