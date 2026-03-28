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
#include <shellapi.h>
#include <string>
#include "uiaccess.h"
#include "Resource.h"
#include "WinUtils/WinUtils.h"
using namespace std;
using namespace WinUtils;

constexpr UINT WM_CUSTOM_SET_TOPMOST = WM_USER + 66;     // 设置窗口置顶
constexpr UINT WM_TIME_RESET_ESC_COUNT = WM_USER + 67;   // 重置ESC按钮计数
constexpr DWORD TIMER_INTERVAL_TOPMOST = 50;             // 置顶检查定时器间隔
constexpr DWORD TIMER_INTERVAL_SEEWO_CHECK = 1000;       // 希沃管家检查定时器间隔
constexpr DWORD TIMER_INTERVAL_ESC_RESET = 2000;         // ESC计数重置定时器间隔
constexpr DWORD TIMER_INTERVAL_MOUSE_DRAG = 50;          // 鼠标拖动定时器间隔
constexpr UINT WINDOW_POS_FLAGS = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;// 窗口样式常量
constexpr int MAX_ESC_CLICK_COUNT = 3;                   // ESC按钮最大点击次数
const wchar_t* SEEWO_MANAGER_WINDOW_NAME = L"希沃管家";  // 目标窗口名称

static HINSTANCE g_hAppInstance = nullptr;               // 应用实例句柄
static HWND g_hMainDialog = nullptr;                     // 主对话框句柄
static BOOL g_bHasUIAccess = FALSE;                      // 是否拥有UIAccess权限
static BOOL g_bAlwaysOnTop = TRUE;                       // 是否始终置顶
static HANDLE g_hTimerDoneEvent = nullptr;               // 定时器完成事件
static HANDLE g_hMainTimerQueue = nullptr;               // 主定时器队列
static HANDLE g_hTopmostCheckTimer = nullptr;            // 置顶检查定时器
static HANDLE g_hMouseDragTimer = nullptr;               // 鼠标拖动定时器
static HANDLE g_hEscResetTimer = nullptr;                // ESC计数重置定时器
static BOOL g_bMouseDragTimerCreated = FALSE;            // 鼠标拖动定时器是否创建
static BOOL g_bEscResetTimerCreated = FALSE;             // ESC重置定时器是否创建
static BOOL g_bSeewoManagerFound = FALSE;                // 是否找到希沃管家窗口
static int g_nEscClickCount = MAX_ESC_CLICK_COUNT;       // ESC按钮点击计数
static POINT g_ptMouseDownClientPos = {};                // 鼠标按下时的客户区坐标
static BOOL g_bMouseLeftButtonDown = FALSE;              // 鼠标左键是否按下

BOOL CALLBACK EnumWindowsFindSeewoManagerProc(HWND hwnd, LPARAM lParam);
VOID CALLBACK TopmostTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired);
VOID CALLBACK MouseDragTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired);
VOID CALLBACK EscResetTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired);
VOID CALLBACK SeewoManagerCheckTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired);
int CreateMainTimerQueue();
INT_PTR CALLBACK MainDialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
int InitializeApplicationInstance(HINSTANCE hInstance);

/**
 * @brief 设置窗口是否始终置顶
 * @param bAlwaysOnTop TRUE=置顶，FALSE=取消置顶
 */
VOID SetWindowAlwaysOnTop(BOOL bAlwaysOnTop)
{
	HWND hwndInsertAfter = bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
	SetWindowPos(g_hMainDialog, hwndInsertAfter, 0, 0, 0, 0, WINDOW_POS_FLAGS);
}

/**
 * @brief 枚举窗口回调函数：查找全屏的"希沃管家"窗口
 * @param hwnd 当前枚举的窗口句柄
 * @param lParam 自定义参数（未使用）
 * @return TRUE=继续枚举，FALSE=停止枚举
 */
BOOL CALLBACK EnumWindowsFindSeewoManagerProc(HWND hwnd, LPARAM lParam)
{
	if (hwnd == g_hMainDialog)return TRUE;
	if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))return TRUE;
	WCHAR szWindowTitle[256] = {};
	GetWindowTextW(hwnd, szWindowTitle, _countof(szWindowTitle));
	wstring wsWindowTitle = szWindowTitle;
	if (wsWindowTitle.empty())return TRUE;
	if (wsWindowTitle == SEEWO_MANAGER_WINDOW_NAME)
	{
		if (IsWindowFullScreen(hwnd, 50, true))
		{
			g_bSeewoManagerFound = TRUE;
			return FALSE; // 找到目标，停止枚举
		}
	}
	return TRUE; // 继续枚举
}

/**
 * @brief 置顶检查定时器回调：确保窗口置顶
 */
VOID CALLBACK TopmostTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	SetWindowAlwaysOnTop(TRUE);
	SetEvent(g_hTimerDoneEvent);
}

/**
 * @brief 鼠标拖动定时器回调：处理鼠标拖动时的消息和光标样式
 */
VOID CALLBACK MouseDragTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	// 发送鼠标移动消息，更新窗口位置
	PostMessage(g_hMainDialog, WM_MOUSEMOVE, 0, 0);

	// 左键松开时发送弹起消息
	if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		PostMessage(g_hMainDialog, WM_LBUTTONUP, 0, 0);

	SetEvent(g_hTimerDoneEvent);
	SetCursor(LoadCursorW(nullptr, IDC_HAND)); // 设置手型光标
}

/**
 * @brief ESC计数重置定时器回调：发送重置计数消息
 */
VOID CALLBACK EscResetTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	PostMessage(g_hMainDialog, WM_TIME_RESET_ESC_COUNT, 0, 0);
	SetEvent(g_hTimerDoneEvent);
	SetCursor(LoadCursorW(nullptr, IDC_HAND));
}

/**
 * @brief 希沃管家检查定时器回调：检测目标窗口并控制自身显示/隐藏
 */
VOID CALLBACK SeewoManagerCheckTimerCallback(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	g_bSeewoManagerFound = FALSE;
	EnumWindows(EnumWindowsFindSeewoManagerProc, 0);

	if (g_hMainDialog != nullptr && IsWindow(g_hMainDialog))
	{
		if (g_bSeewoManagerFound)
		{
			ShowWindow(g_hMainDialog, SW_SHOW);
			SetWindowAlwaysOnTop(g_bAlwaysOnTop);
		}
		else ShowWindow(g_hMainDialog, SW_HIDE);
	}

	SetEvent(g_hTimerDoneEvent);
}

/**
 * @brief 创建主定时器队列和基础定时器
 * @return 0=成功，1=创建事件失败，2=创建定时器队列失败，3=置顶定时器创建失败，4=希沃检查定时器创建失败
 */
int CreateMainTimerQueue()
{
	// 创建定时器完成事件（手动重置，初始无信号）
	g_hTimerDoneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (g_hTimerDoneEvent == nullptr)
		return 1;

	// 创建定时器队列
	g_hMainTimerQueue = CreateTimerQueue();
	if (g_hMainTimerQueue == nullptr)
		return 2;

	// 创建置顶检查定时器（立即执行，间隔50ms）
	if (!CreateTimerQueueTimer(&g_hTopmostCheckTimer, g_hMainTimerQueue,
		(WAITORTIMERCALLBACK)TopmostTimerCallback, nullptr, 0, TIMER_INTERVAL_TOPMOST, 0))
		return 3;

	// 创建希沃管家检查定时器（立即执行，间隔1000ms）
	if (!CreateTimerQueueTimer(&g_hEscResetTimer, g_hMainTimerQueue,
		(WAITORTIMERCALLBACK)SeewoManagerCheckTimerCallback, nullptr, 0, TIMER_INTERVAL_SEEWO_CHECK, 0))
		return 4;

	return 0;
}

/**
 * @brief 主对话框过程函数：处理所有对话框消息
 */
INT_PTR CALLBACK MainDialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	POINT ptCurrentMousePos = {};
	switch (uMsg)
	{
	case WM_COMMAND:
	{
		UINT uControlID = LOWORD(wParam);
		UINT uNotifyCode = HIWORD(wParam);

		switch (uControlID)
		{
		case IDOK:
			EndDialog(hdlg, uControlID);
			break;
		case IDCANCEL:
			EndDialog(hdlg, uControlID);
			break;
		case IDC_MAIN_OPEN:
		{
			wstring wsCurrentDir = GetCurrentProcessDir();
			wstring wsExePath = wsCurrentDir + L"HugoLaunchTool.exe";
			ShellExecuteW(nullptr, nullptr, wsExePath.c_str(), L"-stop", wsCurrentDir.c_str(), SW_SHOWDEFAULT);
			break;
		}
		case IDC_BUTTON_ESC:
			// 减少ESC点击计数并更新按钮文本
			g_nEscClickCount--;
			WCHAR szCountText[10] = {};
			_itow_s(g_nEscClickCount, szCountText, 10);
			SetWindowTextW(GetDlgItem(hdlg, IDC_BUTTON_ESC), szCountText);

			// 计数为0时退出程序
			if (g_nEscClickCount <= 0)
				PostQuitMessage(0);

			if (!g_bEscResetTimerCreated)
			{
				CreateTimerQueueTimer(&g_hEscResetTimer, g_hMainTimerQueue,
					(WAITORTIMERCALLBACK)EscResetTimerCallback, nullptr, TIMER_INTERVAL_ESC_RESET, 0, 0);
				g_bEscResetTimerCreated = TRUE;
			}
			break;
		}
		break;
	}

	case WM_TIME_RESET_ESC_COUNT:
		// 重置ESC点击计数并恢复按钮文本
		g_nEscClickCount = MAX_ESC_CLICK_COUNT;
		SetWindowTextW(GetDlgItem(hdlg, IDC_BUTTON_ESC), TEXT("Esc"));
		if (g_bEscResetTimerCreated)
		{
			BOOL bDeleted = DeleteTimerQueueTimer(g_hMainTimerQueue, g_hEscResetTimer, g_hTimerDoneEvent);
			if (bDeleted)
				g_bEscResetTimerCreated = FALSE;
		}
		break;

	case WM_MOVING:
		// 窗口移动时发送自定义消息确保置顶
		PostMessage(hdlg, WM_CUSTOM_SET_TOPMOST, 0, 0);
		break;

	case WM_CUSTOM_SET_TOPMOST:
		// 响应自定义消息，重新设置置顶状态
		SetWindowAlwaysOnTop(g_bAlwaysOnTop);
		break;

	case WM_INITDIALOG:
		// 初始化对话框：创建定时器、设置样式、保存句柄、置顶
		CreateMainTimerQueue();
		SetWindowLongPtrW(hdlg, GWL_EXSTYLE, GetWindowLongPtrW(hdlg, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
		g_hMainDialog = hdlg;
		SetWindowAlwaysOnTop(g_bAlwaysOnTop);
		return TRUE;

	case WM_LBUTTONDOWN:
		// 鼠标左键按下：标记状态、记录坐标、设置光标、创建拖动定时器
		g_bMouseLeftButtonDown = TRUE;
		GetCursorPos(&ptCurrentMousePos);
		g_ptMouseDownClientPos = ptCurrentMousePos;
		ScreenToClient(hdlg, &g_ptMouseDownClientPos);
		SetCursor(LoadCursorW(nullptr, IDC_HAND));

		if (!g_bMouseDragTimerCreated)
		{
			CreateTimerQueueTimer(&g_hMouseDragTimer, g_hMainTimerQueue,
				(WAITORTIMERCALLBACK)MouseDragTimerCallback, nullptr, 0, TIMER_INTERVAL_MOUSE_DRAG, 0);
			g_bMouseDragTimerCreated = TRUE;
		}
		break;

	case WM_MOUSEMOVE:
		// 鼠标移动且左键按下时，拖动窗口
		if (g_bMouseLeftButtonDown)
		{
			SetCursor(LoadCursorW(nullptr, IDC_HAND));
			GetCursorPos(&ptCurrentMousePos);
			SetWindowPos(hdlg, nullptr,
				ptCurrentMousePos.x - g_ptMouseDownClientPos.x - 1,
				ptCurrentMousePos.y - g_ptMouseDownClientPos.y - 1,
				0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}
		break;

	case WM_LBUTTONUP:
		// 鼠标左键松开：重置状态、恢复光标、删除拖动定时器
		g_bMouseLeftButtonDown = FALSE;
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));

		if (g_bMouseDragTimerCreated)
		{
			BOOL bDeleted = DeleteTimerQueueTimer(g_hMainTimerQueue, g_hMouseDragTimer, g_hTimerDoneEvent);
			if (bDeleted)
				g_bMouseDragTimerCreated = FALSE;
		}
		break;

	default:
		return FALSE;
	}
	return FALSE;
}

/**
 * @brief 初始化应用实例：处理UIAccess、创建对话框、清理资源
 * @param hInstance 应用实例句柄
 * @return 对话框退出码
 */
int InitializeApplicationInstance(HINSTANCE hInstance)
{
	DWORD dwError = 0;
	INT_PTR iDialogResult = 0;
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	dwError = PrepareForUIAccess();
	g_bHasUIAccess = (dwError == ERROR_SUCCESS);
	g_hAppInstance = hInstance;
	iDialogResult = DialogBoxW(g_hAppInstance, MAKEINTRESOURCE(IDD_MAIN), nullptr, MainDialogProc);
	g_hMainDialog = nullptr;

	if (g_hTimerDoneEvent != nullptr)
	{
		WaitForSingleObject(g_hTimerDoneEvent, INFINITE);
		CloseHandle(g_hTimerDoneEvent);
	}
	if (g_hMainTimerQueue != nullptr)
		(void)DeleteTimerQueue(g_hMainTimerQueue);
	CoUninitialize();
	return static_cast<int>(iDialogResult);
}

int APIENTRY _tWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR lpCmdLine,
	_In_ int nCmdShow
)
{
	return InitializeApplicationInstance(hInstance);
}