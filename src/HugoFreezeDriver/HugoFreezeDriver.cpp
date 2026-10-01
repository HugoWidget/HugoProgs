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

#include <iostream>
#include <cstring>
#include <conio.h>
#include <limits>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdlib>
#include <algorithm>

#include "HugoUtils/HugoFreeze/HFreezeDriver.h"
#include "HugoUtils/HugoFreeze/HFreezeDriverEx.h"
#include "HugoUtils/HugoFreeze/HFreezeFileBackend.h"
#include "WinUtils/Console.h"
#include "WinUtils/Logger.h"
#include "WinUtils/WinUtils.h"
#include "WinUtils/CmdParser.h"
#include "WinUtils/StrConvert.h"

using namespace std;
using namespace WinUtils;

// 清屏
void ClearScreen() {
    system("cls");
}

// 将卷掩码转换为盘符列表（例如 0x4 -> "C:"）
wstring VolumeMaskToDrives(uint32_t mask) {
    wstring drives;
    for (int i = 0; i < 26; ++i) {
        if (mask & (1 << i)) {
            if (!drives.empty()) drives += L",";
            drives += wchar_t(L'A' + i);
        }
    }
    return drives.empty() ? L"无" : drives;
}

// 布尔值的可读形式
static const wchar_t* BoolStr(bool value) {
    return value ? L"是" : L"否";
}

// 读取驱动的定长 ASCII 字段（不含结尾的 \0）
static string FixedAscii(const char* data, size_t size) {
    size_t len = 0;
    while (len < size && data[len] != '\0') {
        ++len;
    }
    return string(data, len);
}

// 输出全部底层驱动信息（只读查询，不做任何修改）
void PrintDriverDetails(const HFreezeDriverEx& drv) {
    wcout << L"\n========== 驱动底层详细信息 ==========" << endl;

    // 驱动基础版本
    FreezeBaseVersion version{};
    if (drv.queryBaseVersion(version)) {
        wcout << L"[基础版本] " << (int)version.bytes[0] << L"." << (int)version.bytes[1] << endl;
    }
    else {
        wcout << L"[基础版本] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // 当前拦截开关状态
    FreezeProtectionState protectState{};
    if (drv.queryProtectionState(protectState)) {
        wcout << L"[拦截状态] 驱动状态=0x" << hex << protectState.freezeDriverState << dec
            << L" 停止保护=" << protectState.stopProtect
            << (protectState.stopProtect != 0 ? L" (已放行所有I/O)" : L" (保护中)") << endl;
    }
    else {
        wcout << L"[拦截状态] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // 启动信息与拦截统计
    FreezeEventBootSystem boot{};
    if (drv.queryBootSystem(boot)) {
        const auto& b = boot.data;
        wcout << L"[启动统计] 启动时间=" << b.freezeStartupTime
            << L" 状态机=0x" << hex << b.freezeDriverState << dec << endl;
        wcout << L"  卷就绪=" << b.readyProtectVolume
            << L" 位图比较=" << b.bitmapCompState
            << L" 引导信息校验=" << b.bootInfoRight
            << L" 重入次数=" << b.reinitCallbackTime
            << L" 预取拦截=" << b.prefetchEnable << endl;
        wcout << L"  IRP 原始=" << b.originalIrpCount
            << L" 重定向=" << b.redirectIrpCount
            << L" 算法耗时=" << b.redirectAlgoTime << endl;
        wcout << L"  读取字节=" << b.readBytes
            << L" 写入字节=" << b.writeBytes
            << L" 登录界面退出=" << b.logonUIExitTime << endl;
        wcout << L"  初始化日志: " << ConvertString<wstring>(FixedAscii(b.strDriverInitState, sizeof(b.strDriverInitState))) << endl;
    }
    else {
        wcout << L"[启动统计] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // 底层卷挂载、IRP 劫持等初始化结果
    FreezeKeyResult key{};
    if (drv.queryKeyResult(key)) {
        const auto& k = key.data;
        wcout << L"[初始化结果]" << endl;
        wcout << L"  回调配置   : 蓝屏内存=" << (int)k.freeze_BugcheckDataMem_AllocaSuccess
            << L" 写入尺寸=" << k.freeze_BugcheckDataMem_WriteSize
            << L" 进程回调=" << k.freeze_SetProcessNotify_Status << endl;
        wcout << L"  通信设备   : 创建=" << k.freeze_InitializerInit_ComIoDevCreate
            << L" 附加磁盘=" << k.freeze_InitializerInit_AddDeviceCount
            << L" 已启动=" << k.freeze_InitializerInit_Start << endl;
        wcout << L"  卷配置流程 : 注册表=" << k.freeze_InitializerInit_VolConfig_OpenReg
            << L" 打开键=" << k.freeze_InitializerInit_VolConfig_OpenKey
            << L" 打开文件=" << k.freeze_InitializerInit_VolConfig_Open
            << L" 读取=" << k.freeze_InitializerInit_VolConfig_Read << endl;
        wcout << L"               MD5错误=" << k.freeze_InitializerInit_VolConfig_Md5_Wrong
            << L" 读取成功=" << k.freeze_InitializerInit_VolConfig_ReadSuccess
            << L" 重写=" << k.freeze_InitializerInit_VolConfig_ReWrite
            << L" 完成=" << k.freeze_InitializerInit_VolConfig_Over << endl;
        wcout << L"  环境校验   : Chkdsk=" << BoolStr(k.freeze_InitializerInit_bChkdsk)
            << L" 保存Dump=" << BoolStr(k.freeze_InitializerInit_bCheckDump)
            << L" 停止保护=" << BoolStr(k.freeze_InitializerInit_bStopProtect)
            << L" 需要更新=" << BoolStr(k.freeze_InitializerInit_bNeedUpdate) << endl;
        wcout << L"               就绪卷掩码=0x" << hex << k.freeze_InitializerInit_readyProtectVolume << dec
            << L" 资源初始化=" << k.freeze_InitializerInit_StartInitProtectVolResource << endl;
        wcout << L"  分区结构   : " << (k.freeze_InitializerInit_VolumeInfo_PartitionStyle_Mbr0_Gpt1 ? L"GPT" : L"MBR")
            << L" EBR计算=" << k.freeze_InitializerInit_CalculateEBR
            << L" EBR数量=" << k.freeze_InitializerInit_diskEbrNum
            << L" 解析完成=" << k.freeze_InitializerInit_GetPartitionStyle_Over << endl;
        wcout << L"  资源挂钩   : 卷列表=" << k.freeze_InitializerInit_ProtectVolResource_gVolumeListisValid
            << L" 取卷信息=" << k.freeze_InitializerInit_ProtectVolResource_GetVolumeInfo_Start
            << L" 位图=" << k.freeze_InitializerInit_ProtectVolResource_BitMap_Start
            << L" 穿透配置=" << k.freeze_InitializerInit_passthroughConfigFile << endl;
        wcout << L"               失败卷=" << k.freeze_InitializerInit_ProtectVolResource_FailVolume
            << L" 资源完成=" << k.freeze_InitializerInit_StartInitProtectVolResource_Over
            << L" 磁盘对象=" << k.freeze_InitializerInit_HookDiskMajorFun_GetObject << endl;
        wcout << L"               镜像回调=" << k.freeze_InitializerInit_SetLoadImage
            << L" 回调执行=" << k.freeze_InitializerInit_DriveImageLoadCallBack
            << L" 初始化完成=" << k.freeze_InitializerInit_Over << endl;
        wcout << L"  日志状态   : 已记录错误=" << BoolStr(k.freeze_InitializerInit_errorRecorded)
            << L" 重定向成功=" << BoolStr(k.freeze_InitializerInit_bRedirectSuccess) << endl;
        wcout << L"  文件函数   : " << ConvertString<wstring>(FixedAscii(k.freeze_InitializerInit_FileFuncString, sizeof(k.freeze_InitializerInit_FileFuncString))) << endl;
        wcout << L"  初始化日志 : " << ConvertString<wstring>(FixedAscii(k.freeze_InitializerInit_LogString, sizeof(k.freeze_InitializerInit_LogString))) << endl;
        wcout << L"  错误文件   : " << ConvertString<wstring>(FixedAscii(k.freeze_InitializerInit_ErrorFileString, sizeof(k.freeze_InitializerInit_ErrorFileString))) << endl;
        wcout << L"  盘符磁盘号 : ";
        for (int i = 0; i < static_cast<int>(FRZ_DRIVE_COUNT); ++i) {
            wcout << wchar_t(L'A' + i) << L"=" << (int)k.freeze_AddDevice_getDiskNumber[i] << L" ";
        }
        wcout << L"(状态=" << k.freeze_AddDevice_getDiskNumberStatus << L")" << endl;
        wcout << L"  首次触发   : 读=" << k.freeze_MajorFunction_firstRead
            << L" 写=" << k.freeze_MajorFunction_firstWrite << endl;
        wcout << L"  读写分发器 : 启动=" << k.freeze_ReadWriteHandler_Start
            << L" 非保护更新=" << k.freeze_ReadWriteHandler_Noprotect_Update
            << L" 唯一线程=" << k.freeze_ReadWriteHandler_UniqueThread << endl;
        wcout << L"               引导区=" << k.freeze_ReadWriteHandler_FilterIrpInBootRecord
            << L" 卷有效=" << k.freeze_ReadWriteHandler_Vol_Valid
            << L" 落入保护卷=" << k.freeze_ReadWriteHandler_Irp_InProtectVol
            << L" 事件=" << k.freeze_ReadWriteHandler_KeSetEvent << endl;
        wcout << L"  读写线程   : 启动=" << k.freeze_ReadWriteThread_Start
            << L" 消费队列=" << k.freeze_ReadWriteThread_ConsumeIprQueue
            << L" 磁盘IRP=" << k.freeze_ReadWriteThread_DiskIrpHandler
            << L" 快速FSD=" << k.freeze_ReadWriteThread_FastFsdRequest << endl;
    }
    else {
        wcout << L"[初始化结果] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // ATA/SCSI 底层穿透统计
    FreezeEventPassThrough passThrough{};
    if (drv.queryPassThrough(passThrough)) {
        const auto& p = passThrough.data;
        wcout << L"[穿透统计] ATA 读数据=" << p.ataReadDataSumSectors.QuadPart
            << L" 读分区表=" << p.ataReadPartTableSumSectors.QuadPart
            << L" 写数据=" << p.ataWriteDataSumSectors.QuadPart
            << L" 写分区表=" << p.ataWritePartTableSumSectors.QuadPart << L" (扇区)" << endl;
        wcout << L"           SCSI 读数据=" << p.scsiReadDataSumSectors.QuadPart
            << L" 读分区表=" << p.scsiReadPartTableSumSectors.QuadPart
            << L" 写数据=" << p.scsiWriteDataSumSectors.QuadPart
            << L" 写分区表=" << p.scsiWritePartTableSumSectors.QuadPart << L" (扇区)" << endl;
        wcout << L"           IDE请求=" << p.ideRequestCount
            << L" MPIO请求=" << p.mpioRequestCount << endl;
    }
    else {
        wcout << L"[穿透统计] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // 驱动运行质量异常统计
    FreezeEventOldDriverQuality quality{};
    if (drv.queryOldDriverQuality(quality)) {
        const auto& q = quality.data;
        wcout << L"[质量统计] IRP信息分配失败=" << q.irpInfoAllocFailed
            << L" 内部缓冲分配失败=" << q.interBufAllocFailed
            << L" 缓冲请求尺寸=" << q.interBufAllocSize << endl;
        wcout << L"           映射表校验失败=" << q.checkMapTableFailed
            << L" 映射表插入失败=" << q.insertMapTableFailed
            << L" 位图设置失败=" << q.setBitmapFailed << endl;
        wcout << L"           子IRP失败=" << q.subIrpFailed
            << L" 读写位图失败=" << q.setRWBitmapFailed << endl;
    }
    else {
        wcout << L"[质量统计] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // 各卷空闲扇区与爆盘状态
    FreezeEventDiskFull diskFull{};
    if (drv.queryDiskFull(diskFull)) {
        const auto& d = diskFull.data;
        wcout << L"[卷剩余空间] 卷总数=" << d.volumes << endl;
        for (int i = 0; i < static_cast<int>(FRZ_DRIVE_COUNT); ++i) {
            if (d.volFreeSectorCount[i] == 0) continue;
            wcout << L"  " << wchar_t(L'A' + i) << L": 空闲扇区=" << d.volFreeSectorCount[i] << endl;
        }
    }
    else {
        wcout << L"[卷剩余空间] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // 蓝屏拦截记录
    BsodInfo bsod{};
    if (drv.queryBsodInfo(bsod)) {
        const auto& info = bsod.data;
        wostringstream fingerprintStream;
        for (size_t i = 0; i < sizeof(info.md5); ++i) {
            fingerprintStream << hex << setw(2) << setfill(L'0') << (int)info.md5[i];
        }
        wcout << L"[蓝屏记录] 指纹=" << fingerprintStream.str()
            << L" 发生时间=" << info.bsodTime
            << L" 开机至蓝屏=" << info.startupTimeOccurBsod << endl;
    }
    else {
        wcout << L"[蓝屏记录] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // IRP 重定向队列统计
    FreezeRedirectData redirect{};
    if (drv.queryRedirectData(redirect)) {
        const auto& r = redirect.data;
        wcout << L"[重定向队列] 时间槽=" << r.timeIndex
            << L" 时间基准=" << r.timeBase
            << L" 队列上限=" << r.maxQueueLen << endl;
        wcout << L"             IRP 原始=" << r.originalIrpCount
            << L" 重定向=" << r.redirectIrpCount
            << L" 读取字节=" << r.readBytes
            << L" 写入字节=" << r.writeBytes << endl;
        wcout << L"             完成耗时 最大=" << r.maxIrpCompleteTime
            << L" 平均=" << r.avgIrpCompleteTime << endl;
    }
    else {
        wcout << L"[重定向队列] 查询失败: " << drv.getLastErrorMsg() << endl;
    }

    // 冻结线程池 TID（0x8000205C）虽然能返回全部 TID，但该接口同时会篡改
    // 驱动的重定向队列配置，属于写操作，故不放在只读的详细查询里。
    // 需要时可在菜单中单独执行。

    wcout << L"======================================" << endl;
}

// 格式化并输出冻结状态
void PrintFreezeStatus(const HFreezeDriver& manager) {
    // 获取综合状态
    FreezeResult status = manager.GetFreezeState();
    const auto& disks = status.diskInfos;

    wcout << L"\n========== 希沃驱动冻结状态 ==========" << endl;

    // 盘符状态
    wcout << L"当前冻结状态 (盘符):" << endl;
    if (disks.empty()) {
        wcout << L"  未检测到受保护的盘符" << endl;
    }
    else {
        for (const auto& [letter, info] : disks) {
            wstring stateStr;
            switch (info.state) {
            case DriveFreezeState::Frozen:          stateStr = L"已冻结"; break;
            case DriveFreezeState::Unfrozen:        stateStr = L"未冻结"; break;
            case DriveFreezeState::PendingFreeze:   stateStr = L"待冻结(需重启)"; break;
            case DriveFreezeState::PendingUnfreeze: stateStr = L"待解冻(需重启)"; break;
            default:                                stateStr = L"未知"; break;
            }
            wcout << L"  " << letter << L": " << stateStr << endl;
        }
    }

    // 配置信息（来自 FreezeResult::extra）
    const auto& extra = status.extra;
    wcout << L"\n配置信息:" << endl;
    wcout << L"  MD5          : " << ConvertString<wstring>(extra.md5) << endl;
    wcout << L"  目标冻结掩码 : 0x" << hex << extra.next_mask << dec
        << L" (" << VolumeMaskToDrives(extra.next_mask) << L")" << endl;
    wcout << L"  当前冻结掩码 : 0x" << hex << extra.vol_mask_copy << dec
        << L" (" << VolumeMaskToDrives(extra.vol_mask_copy) << L")" << endl;
    wcout << L"  配置状态     : 0x" << hex << extra.status << dec
        << (extra.status == FRZ_STATUS_FROZEN ? L" (已冻结)" : L" (未冻结)") << endl;
    wcout << L"  写标志       : 0x" << hex << (int)extra.flag1
        << L" / 0x" << (int)extra.flag2 << dec << endl;
    wcout << L"  设备ID       : " << ConvertString<wstring>(extra.device_id) << endl;
    wcout << L"  学校代码     : " << ConvertString<wstring>(extra.school_code) << endl;

    // 运行时信息（如果驱动返回了）
    DriverRuntimeStatus runtime;
    if (manager.QueryDriverStatus(runtime) && runtime.querySuccess) {
        wcout << L"\n运行时状态:" << endl;
        wcout << L"  活动标志     : 0x" << hex << runtime.activeFlag << dec
            << (runtime.activeFlag != 0 ? L" (运行中)" : L" (未激活)") << endl;
        wcout << L"  指针1        : 0x" << hex << runtime.ptr1 << dec << endl;
        if (!runtime.logStr.empty()) {
            wcout << L"  驱动日志     : " << runtime.logStr << endl;
        }
    }
    else {
        wcout << L"\n运行时状态: 未获取到（驱动可能未运行）" << endl;
    }

    if (status.result != FreezeOperationResult::Success) {
        wcout << L"\n查询告警: " << status.errMsg << endl;
    }

    wcout << L"==========================================" << endl;
    WuLog::Info(L"查询了冻结状态");
}

// 设置冻结盘符
void ApplyFreezeSettings(HFreezeDriver& manager, const wstring& drives) {
    wstring target = (drives == L"0" ? L"" : drives);

    uint32_t mask = CalculateVolumeMask(target);
    if (mask == FRZ_VOLUME_MASK_INVALID) {
        wcerr << L"无效盘符输入，操作取消。" << endl;
        WuLog::Warn(L"无效的盘符输入: " + target);
        return;
    }

    FreezeResult result = manager.SetFreezeState(target);
    bool success = (result.result == FreezeOperationResult::Success);

    wcout << L"\n设置冻结盘符 [" << (target.empty() ? L"全部解除" : target) << L"] : "
        << (success ? L"成功" : L"失败") << endl;
    if (!result.msg.empty()) {
        wcout << L"消息: " << result.msg << endl;
    }
    if (!result.errMsg.empty()) {
        wcout << L"错误详情: " << result.errMsg << endl;
    }

    if (success) {
        wcout << L"配置已更新，需要重启计算机才能生效。" << endl;
        WuLog::Info(L"冻结设置成功: " + target);
    }
    else {
        WuLog::Error(L"冻结设置失败: " + target);
    }
}

// 读取整行输入（镜像路径可能包含空格）
// 直接回车返回空串，调用方据此保留原值；不能使用 "wcin >> ws"，
// 它会跳过所有空白并一直等待非空白字符，导致回车无法返回。
static wstring ReadLine(const wchar_t* prompt) {
    wcout << prompt;
    wstring line;
    getline(wcin, line);
    if (wcin.fail()) {
        // 输入流出错（例如 Ctrl+Z）时恢复状态，避免后续读取全部失败
        wcin.clear();
        return wstring();
    }
    while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n')) {
        line.pop_back();
    }
    return line;
}

// 线程池 TID（该接口同时会重置驱动的重定向队列配置，属于写操作）
void QueryThreadPoolTids(const HFreezeDriverEx& drv) {
    FreezeTidRedirectBuffer buffer{};
    if (!drv.queryTidRedirect(buffer)) {
        wcerr << L"查询线程池 TID 失败: " << drv.getLastErrorMsg() << endl;
        WuLog::Error(L"查询线程池 TID 失败");
        return;
    }
    wcout << L"\n线程池 TID（已读取 " << FRZ_DRIVER_REDIRECT_TID_SIZE << L" 字节）:" << endl;
    wcout << FrzHexDump(buffer.data, FRZ_DRIVER_REDIRECT_TID_SIZE) << endl;
    WuLog::Info(L"查询了线程池 TID");
}

// 监控驱动事件通知：注册 5 个事件句柄，等待驱动回调，按任意键结束
void MonitorDriverEvents(HFreezeDriverEx& drv) {
    struct EventSlot {
        const wchar_t* name;
        HANDLE handle;
    };
    EventSlot slots[] = {
        { L"驱动加载", nullptr },
        { L"进程创建", nullptr },
        { L"底层穿透", nullptr },
        { L"驱动质量", nullptr },
        { L"磁盘爆满", nullptr },
    };
    constexpr DWORD slotCount = static_cast<DWORD>(sizeof(slots) / sizeof(slots[0]));

    FreezeEventNotifyHandles handles{};
    handles.data.hEvtDriverLoad = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    handles.data.hEvtProcessCreate = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    handles.data.hEvtPassThrough = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    handles.data.hEvtOldDriverQuality = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    handles.data.hEvtDiskFull = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    slots[0].handle = handles.data.hEvtDriverLoad;
    slots[1].handle = handles.data.hEvtProcessCreate;
    slots[2].handle = handles.data.hEvtPassThrough;
    slots[3].handle = handles.data.hEvtOldDriverQuality;
    slots[4].handle = handles.data.hEvtDiskFull;

    if (!drv.setNotifyHandles(handles)) {
        wcerr << L"注册事件句柄失败: " << drv.getLastErrorMsg() << endl;
        WuLog::Error(L"注册驱动通知句柄失败");
        for (const auto& slot : slots) {
            if (slot.handle) CloseHandle(slot.handle);
        }
        return;
    }

    wcout << L"\n已注册事件句柄，开始监控驱动通知（按任意键结束）..." << endl;
    HANDLE waitHandles[slotCount];
    for (DWORD i = 0; i < slotCount; ++i) {
        waitHandles[i] = slots[i].handle;
    }
    while (!_kbhit()) {
        DWORD index = WaitForMultipleObjects(slotCount, waitHandles, FALSE, 500);
        if (index == WAIT_TIMEOUT) {
            continue;
        }
        if (index == WAIT_FAILED) {
            wcerr << L"等待事件失败，错误码: " << GetLastError() << endl;
            break;
        }
        wcout << L"[通知] " << slots[index].name << L" 事件触发" << endl;
    }
    // 吃掉触发结束的按键，避免被主菜单误当作选项
    _getch();

    // 先注销句柄再关闭，否则驱动会持有已失效的句柄
    FreezeEventNotifyHandles empty{};
    if (!drv.setNotifyHandles(empty)) {
        wcerr << L"注销事件句柄失败: " << drv.getLastErrorMsg() << L"，请勿直接关闭程序。" << endl;
        WuLog::Warn(L"注销驱动通知句柄失败");
    }
    for (const auto& slot : slots) {
        if (slot.handle) CloseHandle(slot.handle);
    }
    wcout << L"监控结束。" << endl;
    WuLog::Info(L"结束驱动事件监控");
}

// 刷新驱动调试日志（WPP）
void FlushDriverLogs(HFreezeDriverEx& drv) {
    if (drv.flushWppLogs()) {
        wcout << L"\n驱动日志已刷新。" << endl;
        WuLog::Info(L"刷新驱动日志成功");
    }
    else {
        wcerr << L"\n刷新驱动日志失败: " << drv.getLastErrorMsg() << endl;
        WuLog::Error(L"刷新驱动日志失败");
    }
}

// 导出驱动中的配置到 .config 文件
void ExportFreezeConfig(const HFreezeDriverEx& drv) {
    wstring path = ReadLine(L"导出路径（回车=当前目录 VolumeInfo.config）: ");
    if (path.empty()) {
        path = GetCurrentProcessDir() + L"VolumeInfo.config";
    }

    unsigned char buffer[FRZ_CONFIG_SIZE] = { 0 };
    if (!drv.getConfig(buffer, sizeof(buffer))) {
        wcerr << L"读取驱动配置失败: " << drv.getLastErrorMsg() << endl;
        WuLog::Error(L"导出配置失败：读取驱动配置失败");
        return;
    }

    HFreezeFileBackend file;
    file.setConfigPath(path);
    if (!file.writeBlob(buffer, sizeof(buffer))) {
        wcerr << L"写入文件失败: " << path << endl;
        WuLog::Error(L"导出配置失败：写入文件失败");
        return;
    }

    wcout << L"已导出 " << FRZ_CONFIG_SIZE << L" 字节到 " << path << endl;
    WuLog::Info(L"导出驱动配置到 " + path);
}

// 从 .config 文件导入配置到驱动
void ImportFreezeConfig(HFreezeDriverEx& drv) {
    wstring path = ReadLine(L"导入路径（回车=当前目录 VolumeInfo.config）: ");
    if (path.empty()) {
        path = GetCurrentProcessDir() + L"VolumeInfo.config";
    }

    HFreezeFileBackend file;
    file.setConfigPath(path);
    unsigned char buffer[FRZ_CONFIG_SIZE] = { 0 };
    if (!file.readBlob(buffer, sizeof(buffer))) {
        wcerr << L"读取配置文件失败（需要 " << FRZ_CONFIG_SIZE << L" 字节）: " << path << endl;
        WuLog::Error(L"导入配置失败：读取文件失败");
        return;
    }

    wcout << L"即将导入以下内容（前 " << FRZ_CONFIG_VALID_LEN << L" 字节）:" << endl;
    wcout << FrzHexDump(buffer, FRZ_CONFIG_VALID_LEN) << endl;
    if (ReadLine(L"确认写入驱动？输入 yes 继续: ") != L"yes") {
        wcout << L"已取消。" << endl;
        return;
    }

    // 只有写入驱动才真正生效
    if (!drv.setConfig(buffer, sizeof(buffer))) {
        wcerr << L"写入驱动失败: " << drv.getLastErrorMsg() << endl;
        WuLog::Error(L"导入配置失败：写入驱动失败");
        return;
    }

    // 一并覆盖 C 盘系统配置文件：驱动在下次启动时读取的就是它
    HFreezeFileBackend systemFile;
    if (!systemFile.writeBlob(buffer, sizeof(buffer))) {
        wcerr << L"错误: 驱动已更新，但覆盖系统配置文件失败: " << FRZ_CONFIG_PATH << endl;
        WuLog::Error(L"导入配置失败：覆盖系统配置文件失败");
        return;
    }

    wcout << L"导入成功，已覆盖驱动配置与 " << FRZ_CONFIG_PATH << L"，重启后生效。" << endl;
    WuLog::Info(L"导入配置: " + path);
}

// 触发蓝屏（危险操作，需要二次确认）
void TriggerBsod(HFreezeDriverEx& drv) {
    wcout << L"\n该操作会让驱动立即触发一次蓝屏（用于验证蓝屏拦截），"
        << L"未保存的数据会丢失。" << endl;
    wstring confirm = ReadLine(L"请输入 BSOD 确认执行: ");
    if (confirm != L"BSOD") {
        wcout << L"已取消。" << endl;
        return;
    }
    if (drv.triggerBsod()) {
        wcout << L"请求已发送（若未立即蓝屏，说明当前配置未启用蓝屏拦截）。" << endl;
        WuLog::Warn(L"触发了驱动蓝屏");
    }
    else {
        wcerr << L"触发失败: " << drv.getLastErrorMsg() << endl;
        WuLog::Error(L"触发驱动蓝屏失败");
    }
}

// 显示帮助
void PrintHelp() {
    wcout << L"用法: HugoFreezeDriver.exe [选项]\n"
        << L"选项:\n"
        << L"  --query              查询当前冻结状态\n"
        << L"  --set <盘符>         设置冻结目标盘符（例如 CD 表示C和D盘，0 表示解除所有）\n"
        << L"  --help, -h           显示本帮助信息\n"
        << L"无参数运行则进入交互菜单\n";
}

int wmain(int argc, wchar_t* argv[]) {
    try {
        RequireAdminPrivilege(true);
        Console().setLocale();

        LoggerCore::Inst().SetDefaultStrategies(L"HugoFreezeDriver.log");
        LoggerCore::Inst().EnableApartment(DftLogger);
        HFreezeDriver manager;
        FreezeResult initRes = manager.Init();
        if (initRes.result != FreezeOperationResult::Success) {
            wcerr << L"打开驱动失败，请检查驱动是否安装或权限是否足够。" << endl;
            wcerr << L"错误信息: " << initRes.errMsg << endl;
            WuLog::Error(L"驱动初始化失败: " + initRes.errMsg);
            return 1;
        }
        struct ManagerGuard {
            HFreezeDriver& mgr;
            ~ManagerGuard() { mgr.Cleanup(); }
        } guard{ manager };
        if (argc > 1) {

            wstring cmdLine;
            for (int i = 1; i < argc; ++i) {
                if (i > 1) cmdLine += L" ";
                cmdLine += argv[i];
            }

            CmdParser parser(true);
            if (!parser.parse(cmdLine)) {
                wcerr << L"命令行解析失败" << endl;
                return 1;
            }

            if (parser.hasCommand(L"help") || parser.hasCommand(L"-h")) {
                PrintHelp();
                return 0;
            }

            if (parser.hasCommand(L"query")) {
                PrintFreezeStatus(manager);
                return 0;
            }

            if (parser.hasCommand(L"set")) {
                auto params = parser.getParams(L"set");
                if (params.empty()) {
                    wcerr << L"错误：--set 需要盘符参数" << endl;
                    return 1;
                }
                ApplyFreezeSettings(manager, params[0]);
                return 0;
            }

            wcerr << L"未知选项，请使用 --help 查看帮助。" << endl;
            return 1;
        }

        // 交互菜单使用的驱动扩展句柄：与 manager 共用同一设备句柄（引用计数）
        HFreezeDriverEx driverEx;
        bool driverExReady = driverEx.open();
        if (!driverExReady) {
            wcerr << L"警告: 驱动扩展接口打开失败，部分菜单功能不可用。" << endl;
        }
        struct DriverExGuard {
            HFreezeDriverEx& drv;
            bool active;
            ~DriverExGuard() { if (active) drv.close(); }
        } driverExGuard{ driverEx, driverExReady };

        int choice = -1;
        do {
            ClearScreen();
            wcout << L"\n=== 希沃驱动冻结工具 ===" << endl;
            wcout << L"1. 查询冻结状态" << endl;
            wcout << L"2. 设置冻结盘符" << endl;
            wcout << L"3. 查询底层驱动详细信息" << endl;
            wcout << L"4. 查询线程池 TID（会重置驱动重定向队列配置）" << endl;
            wcout << L"5. 导出配置文件" << endl;
            wcout << L"6. 导入配置文件" << endl;
            wcout << L"7. 监控驱动事件通知" << endl;
            wcout << L"8. 刷新驱动日志" << endl;
            wcout << L"9. 触发蓝屏（危险操作）" << endl;
            wcout << L"0. 退出程序" << endl;
            wcout << L"请输入选择: ";
            wcin >> choice;

            if (wcin.fail()) {
                wcin.clear();
                wcin.ignore(1024, L'\n');
                wcerr << L"输入无效，请输入数字。" << endl;
                system("pause");
                continue;
            }
            // 丢弃选项行剩余内容（含换行），保证后续按行读取的输入不被空行污染
            wcin.ignore((numeric_limits<streamsize>::max)(), L'\n');

            // 3~9 号菜单全部依赖驱动扩展接口
            if (choice >= 3 && choice <= 9 && !driverExReady) {
                wcerr << L"驱动扩展接口未打开，该操作不可用。" << endl;
                system("pause");
                continue;
            }

            switch (choice) {
            case 1:
                PrintFreezeStatus(manager);
                break;
            case 2: {
                wstring input;
                wcout << L"请输入要冻结的盘符（如 CD 表示C和D盘），输入 0 解除所有冻结: ";
                wcin >> input;
                ApplyFreezeSettings(manager, input);
                break;
            }
            case 3:
                PrintDriverDetails(driverEx);
                break;
            case 4:
                QueryThreadPoolTids(driverEx);
                break;
            case 5:
                ExportFreezeConfig(driverEx);
                break;
            case 6:
                ImportFreezeConfig(driverEx);
                break;
            case 7:
                MonitorDriverEvents(driverEx);
                break;
            case 8:
                FlushDriverLogs(driverEx);
                break;
            case 9:
                TriggerBsod(driverEx);
                break;
            case 0:
                wcout << L"正在退出程序..." << endl;
                WuLog::Info(L"用户退出程序");
                break;
            default:
                wcerr << L"无效选择，请重新输入。" << endl;
                break;
            }

            if (choice != 0) {
                system("pause");
            }
        } while (choice != 0);

        return 0;
    }
    catch (const exception& e) {
        wcerr << L"发生致命错误: " << ConvertString<wstring>(e.what()) << endl;
        WuLog::Error(L"致命异常: " + ConvertString<wstring>(e.what()));
        return 1;
    }
}