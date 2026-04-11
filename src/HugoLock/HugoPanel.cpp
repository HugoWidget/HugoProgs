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
#include <windows.h>
#include <tchar.h>
#include <string>
#include "WinUtils/WinUtils.h"
#include "HugoUtils/HugoLock.h"
#include "Resource.h"
#include "HugoLock.h"

using namespace WinUtils;

constexpr WCHAR  CHAR_CROSS[] = L"✕";
constexpr WCHAR  CHAR_CHECK[] = L"✓";
constexpr UINT WM_CUSTOM_SET_TOPMOST = WM_USER + 66;
constexpr UINT WM_TIME_RESET_ESC_COUNT = WM_USER + 67;
constexpr DWORD TIMER_INTERVAL_TOPMOST = 50;
constexpr DWORD TIMER_INTERVAL_SEEWO_CHECK = 1000;
constexpr DWORD TIMER_INTERVAL_ESC_RESET = 2000;
constexpr DWORD TIMER_INTERVAL_MOUSE_DRAG = 50;
constexpr UINT WINDOW_POS_FLAGS = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
constexpr int MAX_ESC_CLICK_COUNT = 3;

static HWND g_hMainDialog = nullptr;
static BOOL g_bAlwaysOnTop = TRUE;
static HANDLE g_hTimerDoneEvent = nullptr;
static HANDLE g_hMainTimerQueue = nullptr;
static HANDLE g_hTopmostCheckTimer = nullptr;
static HANDLE g_hMouseDragTimer = nullptr;
static HANDLE g_hEscResetTimer = nullptr;
static HANDLE g_hSeewoCheckTimer = nullptr;
static BOOL g_bMouseDragTimerCreated = FALSE;
static BOOL g_bEscResetTimerCreated = FALSE;
static int g_nEscClickCount = MAX_ESC_CLICK_COUNT;
static POINT g_ptMouseDownClientPos = {};
static BOOL g_bMouseLeftButtonDown = FALSE;
static BOOL g_bSeewoManagerFound = FALSE;


static void setLayeredAttribute(HWND hWnd) {
	//SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	//SetLayeredWindowAttributes(hWnd, RGB(100,100,100), 40, LWA_ALPHA | LWA_COLORKEY);
}
// 获取当前对话框上的主按钮句柄
static HWND GetMainButton() {
	HWND hBtn = GetDlgItem(g_hMainDialog, IDC_MAIN_OPEN);
	if (!hBtn) hBtn = GetDlgItem(g_hMainDialog, IDC_CLOSE_BUTTON);
	return hBtn;
}

// 检测是否存在全屏希沃管家窗口
static BOOL IsSeewoManagerFullscreen() {
	BOOL found = FALSE;
	EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
		if (hwnd == g_hMainDialog) return TRUE;
		if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) return TRUE;
		if (GetWindowTitleString(hwnd) == L"希沃管家" && IsWindowFullScreen(hwnd, 100, true)) {
			*reinterpret_cast<BOOL*>(lParam) = TRUE;
			return FALSE;
		}
		return TRUE;
		}, reinterpret_cast<LPARAM>(&found));
	return found;
}

// 定时器回调：检测希沃管家，控制对话框显示/隐藏
VOID CALLBACK SeewoManagerCheckTimerCallback(PVOID, BOOLEAN) {
	g_bSeewoManagerFound = IsSeewoManagerFullscreen();
	if (g_hMainDialog && IsWindow(g_hMainDialog)) {
		if (g_bSeewoManagerFound) {
			ShowWindow(g_hMainDialog, SW_SHOW);
			SetWindowPos(g_hMainDialog, HWND_TOPMOST, 0, 0, 0, 0, WINDOW_POS_FLAGS);
		}
		else {
			ShowWindow(g_hMainDialog, SW_HIDE);
		}
	}
	SetEvent(g_hTimerDoneEvent);
}

VOID CALLBACK TopmostTimerCallback(PVOID, BOOLEAN) {
	if (g_hMainDialog && IsWindow(g_hMainDialog)) {
		SetWindowPos(g_hMainDialog, HWND_TOPMOST, 0, 0, 0, 0, WINDOW_POS_FLAGS);

		HWND hBtn = GetMainButton();
		if (hBtn) {
			BOOL bIntercept = GetInterceptEnabled();
			if (!bIntercept) {
				EnableWindow(hBtn, TRUE);
				SetWindowTextW(hBtn, CHAR_CROSS);
			}
			else {
				if (IsSeewoManagerFullscreen()) {
					EnableWindow(hBtn, TRUE);
					SetWindowTextW(hBtn, L"-");
				}
			}
		}
	}
	SetEvent(g_hTimerDoneEvent);
}

VOID CALLBACK MouseDragTimerCallback(PVOID, BOOLEAN) {
	PostMessage(g_hMainDialog, WM_MOUSEMOVE, 0, 0);
	if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		PostMessage(g_hMainDialog, WM_LBUTTONUP, 0, 0);
	SetEvent(g_hTimerDoneEvent);
	SetCursor(LoadCursorW(nullptr, IDC_HAND));
}

VOID CALLBACK EscResetTimerCallback(PVOID, BOOLEAN) {
	PostMessage(g_hMainDialog, WM_TIME_RESET_ESC_COUNT, 0, 0);
	SetEvent(g_hTimerDoneEvent);
	SetCursor(LoadCursorW(nullptr, IDC_HAND));
}

static int CreateCommonTimers() {
	if (!g_hTimerDoneEvent) {
		g_hTimerDoneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		if (!g_hTimerDoneEvent) return 1;
	}
	if (!g_hMainTimerQueue) {
		g_hMainTimerQueue = CreateTimerQueue();
		if (!g_hMainTimerQueue) return 2;
	}
	if (!CreateTimerQueueTimer(&g_hTopmostCheckTimer, g_hMainTimerQueue,
		TopmostTimerCallback, nullptr, 0, TIMER_INTERVAL_TOPMOST, 0))
		return 3;
	if (!CreateTimerQueueTimer(&g_hSeewoCheckTimer, g_hMainTimerQueue,
		SeewoManagerCheckTimerCallback, nullptr, 0, TIMER_INTERVAL_SEEWO_CHECK, 0))
		return 4;
	return 0;
}

INT_PTR CALLBACK SimpleDialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		SetWindowLongPtrW(hdlg, GWL_EXSTYLE, GetWindowLongPtrW(hdlg, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
		setLayeredAttribute(hdlg);
		g_hMainDialog = hdlg;
		CreateCommonTimers();
		ShowWindow(hdlg, IsSeewoManagerFullscreen() ? SW_SHOW : SW_HIDE);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_CLOSE_BUTTON) {
			SetInterceptEnabled(true);
			HWND hBtn = GetMainButton();
			EnableWindow(hBtn, TRUE);
			SetWindowTextW(hBtn, CHAR_CHECK);
			UnlockOnce();
			return TRUE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hdlg, 0);
		return TRUE;
	}
	return FALSE;
}

INT_PTR CALLBACK MainDialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	POINT pt;
	switch (uMsg) {
	case WM_INITDIALOG:
		CreateCommonTimers();
		SetWindowLongPtrW(hdlg, GWL_EXSTYLE, GetWindowLongPtrW(hdlg, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
		setLayeredAttribute(hdlg);
		g_hMainDialog = hdlg;
		SetWindowPos(hdlg, HWND_TOPMOST, 0, 0, 0, 0, WINDOW_POS_FLAGS);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			EndDialog(hdlg, LOWORD(wParam));
			break;
		case IDC_MAIN_OPEN:
		{
			SetInterceptEnabled(true);
			UnlockOnce();
			HWND hBtn = GetMainButton();
			EnableWindow(hBtn, TRUE);
			SetWindowTextW(hBtn, CHAR_CHECK);
			break;
		}
		case IDC_BUTTON_ESC:
		{
			g_nEscClickCount--;
			WCHAR buf[10] = {};
			_itow_s(g_nEscClickCount, buf, 10);
			SetWindowTextW(GetDlgItem(hdlg, IDC_BUTTON_ESC), buf);
			if (g_nEscClickCount <= 0)
				PostQuitMessage(0);
			if (!g_bEscResetTimerCreated) {
				CreateTimerQueueTimer(&g_hEscResetTimer, g_hMainTimerQueue,
					EscResetTimerCallback, nullptr, TIMER_INTERVAL_ESC_RESET, 0, 0);
				g_bEscResetTimerCreated = TRUE;
			}
			break;
		}
		}
		break;

	case WM_TIME_RESET_ESC_COUNT:
		g_nEscClickCount = MAX_ESC_CLICK_COUNT;
		SetWindowTextW(GetDlgItem(hdlg, IDC_BUTTON_ESC), L"Esc");
		if (g_bEscResetTimerCreated) {
			DeleteTimerQueueTimer(g_hMainTimerQueue, g_hEscResetTimer, g_hTimerDoneEvent);
			g_bEscResetTimerCreated = FALSE;
		}
		break;

	case WM_MOVING:
		PostMessage(hdlg, WM_CUSTOM_SET_TOPMOST, 0, 0);
		break;

	case WM_CUSTOM_SET_TOPMOST:
		SetWindowPos(hdlg, HWND_TOPMOST, 0, 0, 0, 0, WINDOW_POS_FLAGS);
		break;

	case WM_LBUTTONDOWN:
		g_bMouseLeftButtonDown = TRUE;
		GetCursorPos(&pt);
		g_ptMouseDownClientPos = pt;
		ScreenToClient(hdlg, &g_ptMouseDownClientPos);
		SetCursor(LoadCursorW(nullptr, IDC_HAND));
		if (!g_bMouseDragTimerCreated) {
			CreateTimerQueueTimer(&g_hMouseDragTimer, g_hMainTimerQueue,
				MouseDragTimerCallback, nullptr, 0, TIMER_INTERVAL_MOUSE_DRAG, 0);
			g_bMouseDragTimerCreated = TRUE;
		}
		break;

	case WM_MOUSEMOVE:
		if (g_bMouseLeftButtonDown) {
			SetCursor(LoadCursorW(nullptr, IDC_HAND));
			GetCursorPos(&pt);
			SetWindowPos(hdlg, nullptr,
				pt.x - g_ptMouseDownClientPos.x - 1,
				pt.y - g_ptMouseDownClientPos.y - 1,
				0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
		break;

	case WM_LBUTTONUP:
		g_bMouseLeftButtonDown = FALSE;
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
		if (g_bMouseDragTimerCreated) {
			DeleteTimerQueueTimer(g_hMainTimerQueue, g_hMouseDragTimer, g_hTimerDoneEvent);
			g_bMouseDragTimerCreated = FALSE;
		}
		break;

	default:
		return FALSE;
	}
	return FALSE;
}