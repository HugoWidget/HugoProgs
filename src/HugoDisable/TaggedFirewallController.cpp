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
#include "TaggedFirewallController.h"

TaggedFirewallController::TaggedFirewallController(const ControllerIdentity& identity)
    : identity_(identity), initialized_(false), comInitialized_(false),
    policy_(nullptr), rules_(nullptr), monitorThread_(nullptr),
    monitorRunning_(false), monitorInterval_(300) {

    InitializeCriticalSection(&domainCs_);
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (identity_.createdTime == 0) {
        identity_.createdTime = GetCurrentTimestamp();
    }
    if (identity_.instanceId.empty()) {
        UUID uuid;
        UuidCreate(&uuid);
        RPC_WSTR rpcStr;
        UuidToStringW(&uuid, &rpcStr);
        identity_.instanceId = std::wstring((wchar_t*)rpcStr).substr(0, 4);
        RpcStringFreeW(&rpcStr);
    }
}

TaggedFirewallController::~TaggedFirewallController() {
    StopDomainMonitor();
    Shutdown();
    DeleteCriticalSection(&domainCs_);
    WSACleanup();
}

TaggedFirewallController& TaggedFirewallController::GetDefaultInstance() {
    static ControllerIdentity defaultId;
    defaultId.appName = L"NetBlocker";
    defaultId.version = L"1.0";
    static TaggedFirewallController instance(defaultId);
    return instance;
}

bool TaggedFirewallController::Initialize() {
    if (initialized_) return true;

    // 初始化COM
    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    comInitialized_ = SUCCEEDED(hr) || hr == S_OK;

    // 创建NetFwPolicy2实例
    hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
        __uuidof(INetFwPolicy2), (void**)&policy_);
    if (FAILED(hr)) {
        if (comInitialized_) CoUninitialize();
        return false;
    }

    // 获取规则集合
    hr = policy_->get_Rules(&rules_);
    if (FAILED(hr)) {
        policy_->Release();
        policy_ = nullptr;
        if (comInitialized_) CoUninitialize();
        return false;
    }

   

    // 配置防火墙策略
    if (!InitializeFirewallPolicy()) {
        rules_->Release();
        policy_->Release();
        rules_ = nullptr;
        policy_ = nullptr;
        if (comInitialized_) CoUninitialize();
        return false;
    }

    initialized_ = true;
    return true;
}

void TaggedFirewallController::Shutdown() {
    if (rules_) {
        rules_->Release();
        rules_ = nullptr;
    }
    if (policy_) {
        policy_->Release();
        policy_ = nullptr;
    }
    if (comInitialized_) {
        CoUninitialize();
        comInitialized_ = false;
    }
    initialized_ = false;
}

bool TaggedFirewallController::EnableFirewall() {
    if (!policy_) return false;

    // 启用所有配置文件
    HRESULT hr = policy_->put_FirewallEnabled(NET_FW_PROFILE2_DOMAIN, VARIANT_TRUE);
    if (FAILED(hr)) return false;
    hr = policy_->put_FirewallEnabled(NET_FW_PROFILE2_PRIVATE, VARIANT_TRUE);
    if (FAILED(hr)) return false;
    hr = policy_->put_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, VARIANT_TRUE);
    return SUCCEEDED(hr);
}

bool TaggedFirewallController::DisableFirewall() {
    if (!policy_) return false;

    HRESULT hr = policy_->put_FirewallEnabled(NET_FW_PROFILE2_DOMAIN, VARIANT_FALSE);
    if (FAILED(hr)) return false;
    hr = policy_->put_FirewallEnabled(NET_FW_PROFILE2_PRIVATE, VARIANT_FALSE);
    if (FAILED(hr)) return false;
    hr = policy_->put_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, VARIANT_FALSE);
    return SUCCEEDED(hr);
}

bool TaggedFirewallController::SetDefaultInboundAction(bool allow) {
    if (!policy_) return false;

    NET_FW_ACTION action = allow ? NET_FW_ACTION_ALLOW : NET_FW_ACTION_BLOCK;

    HRESULT hr = policy_->put_DefaultInboundAction(NET_FW_PROFILE2_DOMAIN, action);
    if (FAILED(hr)) return false;
    hr = policy_->put_DefaultInboundAction(NET_FW_PROFILE2_PRIVATE, action);
    if (FAILED(hr)) return false;
    hr = policy_->put_DefaultInboundAction(NET_FW_PROFILE2_PUBLIC, action);
    return SUCCEEDED(hr);
}

bool TaggedFirewallController::SetDefaultOutboundAction(bool allow) {
    if (!policy_) return false;

    NET_FW_ACTION action = allow ? NET_FW_ACTION_ALLOW : NET_FW_ACTION_BLOCK;

    HRESULT hr = policy_->put_DefaultOutboundAction(NET_FW_PROFILE2_DOMAIN, action);
    if (FAILED(hr)) return false;
    hr = policy_->put_DefaultOutboundAction(NET_FW_PROFILE2_PRIVATE, action);
    if (FAILED(hr)) return false;
    hr = policy_->put_DefaultOutboundAction(NET_FW_PROFILE2_PUBLIC, action);
    return SUCCEEDED(hr);
}

bool TaggedFirewallController::IsFirewallEnabled() {
    if (!policy_) return false;

    VARIANT_BOOL enabled = VARIANT_FALSE;
    policy_->get_FirewallEnabled(NET_FW_PROFILE2_ALL, &enabled);
    return enabled == VARIANT_TRUE;
}

bool TaggedFirewallController::InitializeFirewallPolicy() {
    if (!EnableFirewall()) return false;
    if (!SetDefaultInboundAction(false)) return false;
    if (!SetDefaultOutboundAction(true)) return false;
    return true;
}

std::wstring TaggedFirewallController::SanitizeName(const std::wstring& input) {
    std::wstring result;
    for (wchar_t c : input) {
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
            (c >= L'0' && c <= L'9') || c == L'_' || c == L'-' || c == L'.') {
            result += c;
        }
        else if (c == L'\\' || c == L'/' || c == L' ' || c == L'|') {
            result += L'_';
        }
    }
    if (result.length() > 50) {
        result = result.substr(0, 50);
    }
    return result;
}

std::wstring TaggedFirewallController::GenerateRuleName(
    const std::wstring& programPath,
    const std::wstring& targetSpecifier) {
    std::wstring hashInput = programPath + L"|" + targetSpecifier;
    std::wstring md5 = ComputeMD5(hashInput);
    md5 = md5.substr(0, 16);

    std::wstring safeApp = SanitizeName(identity_.appName);
    std::wstring safeInstance = SanitizeName(identity_.instanceId);

    std::wstring result = std::wstring(NAME_PREFIX) + safeApp + L"_" + safeInstance + L"_" + md5;
    if (result.length() > 100) {
        result = result.substr(0, 100);
    }
	return result;
}

bool TaggedFirewallController::ParseRuleName(const std::wstring& ruleName, RuleMetadata& outMeta) {
    if (ruleName.find(NAME_PREFIX) != 0) return false;

    std::wstring content = ruleName.substr(wcslen(NAME_PREFIX));
    size_t firstUnderscore = content.find(L'_');
    if (firstUnderscore == std::wstring::npos) return false;
    std::wstring app = content.substr(0, firstUnderscore);

    size_t secondUnderscore = content.find(L'_', firstUnderscore + 1);
    if (secondUnderscore == std::wstring::npos) return false;
    std::wstring instance = content.substr(firstUnderscore + 1, secondUnderscore - firstUnderscore - 1);

    std::wstring rest = content.substr(secondUnderscore + 1);
    size_t suffixPos = rest.find(L'_');
    std::wstring md5 = (suffixPos == std::wstring::npos) ? rest : rest.substr(0, suffixPos);

    if (md5.length() != 16) return false;
    for (wchar_t c : md5) {
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'))) {
            return false;
        }
    }

    outMeta.ownerApp = app;
    outMeta.ownerInstance = instance;
    outMeta.ruleName = ruleName;

    return true;
}

bool TaggedFirewallController::CleanupResidualRules() {
    if (!rules_) return false;

    auto allRules = EnumerateAllRules();
    std::wstring prefix = std::wstring(NAME_PREFIX) + SanitizeName(identity_.appName) + L"_";

    int count = 0;
    for (const auto& rule : allRules) {
        if (rule.ruleName.find(prefix) == 0) {
            RemoveRule(rule.ruleName);
            count++;
        }
    }
	return true;
}

ULONGLONG TaggedFirewallController::GetCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::wstring TaggedFirewallController::ProtocolToString(Protocol p) {
    switch (p) {
    case Protocol::TCP: return L"tcp";
    case Protocol::UDP: return L"udp";
   // case Protocol::ICMP: return L"icmpv4";
    default: return L"any";
    }
}

std::wstring TaggedFirewallController::DirectionToString(Direction d) {
    switch (d) {
    case Direction::INBOUND: return L"in";
    case Direction::OUTBOUND: return L"out";
    default: return L"out";
    }
}

TaggedFirewallController::Protocol TaggedFirewallController::IntToProtocol(LONG proto) {
    switch (proto) {
    case NET_FW_IP_PROTOCOL_TCP: return Protocol::TCP;
    case NET_FW_IP_PROTOCOL_UDP: return Protocol::UDP;
    //case NET_FW_IP_PROTOCOL_ICMPv4: return Protocol::ICMP;
    default: return Protocol::ANY;
    }
}

TaggedFirewallController::Direction TaggedFirewallController::IntToDirection(NET_FW_RULE_DIRECTION dir) {
    switch (dir) {
    case NET_FW_RULE_DIR_IN: return Direction::INBOUND;
    case NET_FW_RULE_DIR_OUT: return Direction::OUTBOUND;
    default: return Direction::OUTBOUND;
    }
}

std::vector<std::wstring> TaggedFirewallController::ResolveDomain(const std::wstring& domain) {
    std::vector<std::wstring> ips;

    ADDRINFOW hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    PADDRINFOW result = nullptr;

    if (GetAddrInfoW(domain.c_str(), NULL, &hints, &result) == 0) {
        for (PADDRINFOW ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            WCHAR ipStr[INET6_ADDRSTRLEN] = {};

            if (ptr->ai_family == AF_INET) {
                SOCKADDR_IN* sockaddr = reinterpret_cast<SOCKADDR_IN*>(ptr->ai_addr);
                InetNtopW(AF_INET, &sockaddr->sin_addr, ipStr, INET6_ADDRSTRLEN);
            }
            else if (ptr->ai_family == AF_INET6) {
                SOCKADDR_IN6* sockaddr = reinterpret_cast<SOCKADDR_IN6*>(ptr->ai_addr);
                InetNtopW(AF_INET6, &sockaddr->sin6_addr, ipStr, INET6_ADDRSTRLEN);
            }

            if (wcslen(ipStr) > 0) {
                ips.push_back(ipStr);
            }
        }
        FreeAddrInfoW(result);
    }

    return ips;
}

bool TaggedFirewallController::AddRule(const RuleMetadata& meta) {
    if (!rules_) return false;

    INetFwRule* newRule = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(NetFwRule), NULL, CLSCTX_INPROC_SERVER,
        __uuidof(INetFwRule), (void**)&newRule);
    if (FAILED(hr)) return false;

    // 设置名称
    BSTR bstrName = SysAllocString(meta.ruleName.c_str());
    newRule->put_Name(bstrName);
    SysFreeString(bstrName);

    // 设置描述（可选，包含可读信息）
    std::wstring desc = L"NetBlocker rule for " + meta.targetProgram;
    BSTR bstrDesc = SysAllocString(desc.c_str());
    newRule->put_Description(bstrDesc);
    SysFreeString(bstrDesc);

    // 设置程序路径
    if (!meta.targetProgram.empty()) {
        BSTR bstrProg = SysAllocString(meta.targetProgram.c_str());
        newRule->put_ApplicationName(bstrProg);
        SysFreeString(bstrProg);
    }

    // 设置方向
    newRule->put_Direction((NET_FW_RULE_DIRECTION)meta.direction);

    // 设置动作（阻止）
    newRule->put_Action(NET_FW_ACTION_BLOCK);

    // 设置启用状态
    newRule->put_Enabled(meta.enabled ? VARIANT_TRUE : VARIANT_FALSE);

    // 设置协议
    newRule->put_Protocol((NET_FW_IP_PROTOCOL)meta.protocol);

    // 设置远程地址
    if (!meta.remoteAddresses.empty() && meta.remoteAddresses != L"any") {
        BSTR bstrAddr = SysAllocString(meta.remoteAddresses.c_str());
        newRule->put_RemoteAddresses(bstrAddr);
        SysFreeString(bstrAddr);
    }

    // 设置远程端口
    if (!meta.remotePorts.empty() && meta.remotePorts != L"any") {
        BSTR bstrPort = SysAllocString(meta.remotePorts.c_str());
        newRule->put_RemotePorts(bstrPort);
        SysFreeString(bstrPort);
    }

    // 添加到规则集合
    hr = rules_->Add(newRule);
    newRule->Release();

    return SUCCEEDED(hr);
}

bool TaggedFirewallController::RemoveRule(const std::wstring& ruleName) {
    if (!rules_) return false;

    BSTR bstrName = SysAllocString(ruleName.c_str());
    HRESULT hr = rules_->Remove(bstrName);
    SysFreeString(bstrName);

    return SUCCEEDED(hr);
}

bool TaggedFirewallController::SetRuleEnabled(const std::wstring& ruleName, bool enabled) {
    if (!rules_) return false;

    INetFwRule* rule = nullptr;
    BSTR bstrName = SysAllocString(ruleName.c_str());
    HRESULT hr = rules_->Item(bstrName, &rule);
    SysFreeString(bstrName);

    if (FAILED(hr)) return false;

    rule->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
    rule->Release();
    return true;
}

// ==================== 规则枚举 ====================

std::vector<TaggedFirewallController::RuleMetadata> TaggedFirewallController::EnumerateAllRules() {
    std::vector<RuleMetadata> result;
    if (!rules_) return result;

    IUnknown* pEnumerator = nullptr;
    HRESULT hr = rules_->get__NewEnum(&pEnumerator);
    if (FAILED(hr)) return result;

    IEnumVARIANT* pEnumVariant = nullptr;
    hr = pEnumerator->QueryInterface(__uuidof(IEnumVARIANT), (void**)&pEnumVariant);
    pEnumerator->Release();
    if (FAILED(hr)) return result;

    ULONG fetched = 0;
    VARIANT var;

    while (pEnumVariant->Next(1, &var, &fetched) == S_OK) {
        if (var.vt == VT_DISPATCH) {
            INetFwRule* rule = nullptr;
            if (SUCCEEDED(var.pdispVal->QueryInterface(__uuidof(INetFwRule), (void**)&rule))) {
                RuleMetadata meta;
                if (ExtractMetadataFromRule(rule, meta)) {
                    result.push_back(meta);
                }
                rule->Release();
            }
        }
        VariantClear(&var);
    }

    pEnumVariant->Release();
    return result;
}

bool TaggedFirewallController::ExtractMetadataFromRule(INetFwRule* rule, RuleMetadata& outMeta) {
    if (!rule) return false;

    BSTR bstrVal = nullptr;

    // 获取名称
    if (SUCCEEDED(rule->get_Name(&bstrVal)) && bstrVal) {
        outMeta.ruleName = bstrVal;
        SysFreeString(bstrVal);
        bstrVal = nullptr;
    }
    else {
        return false;
    }

    // 解析名称获取元数据
    ParseRuleName(outMeta.ruleName, outMeta);

    // 获取程序路径
    if (SUCCEEDED(rule->get_ApplicationName(&bstrVal)) && bstrVal) {
        outMeta.targetProgram = bstrVal;
        SysFreeString(bstrVal);
        bstrVal = nullptr;
    }

    // 获取方向
    NET_FW_RULE_DIRECTION dir;
    if (SUCCEEDED(rule->get_Direction(&dir))) {
        outMeta.direction = IntToDirection(dir);
    }

    // 获取协议
    LONG proto;
    if (SUCCEEDED(rule->get_Protocol(&proto))) {
        outMeta.protocol = IntToProtocol(proto);
    }

    // 获取远程地址
    if (SUCCEEDED(rule->get_RemoteAddresses(&bstrVal)) && bstrVal) {
        outMeta.remoteAddresses = bstrVal;
        SysFreeString(bstrVal);
        bstrVal = nullptr;
    }

    // 获取远程端口
    if (SUCCEEDED(rule->get_RemotePorts(&bstrVal)) && bstrVal) {
        outMeta.remotePorts = bstrVal;
        SysFreeString(bstrVal);
        bstrVal = nullptr;
    }

    // 获取启用状态
    VARIANT_BOOL enabled;
    if (SUCCEEDED(rule->get_Enabled(&enabled))) {
        outMeta.enabled = (enabled == VARIANT_TRUE);
    }

    return true;
}

std::vector<TaggedFirewallController::RuleMetadata> TaggedFirewallController::GetAllRules() {
    return EnumerateAllRules();
}

std::vector<TaggedFirewallController::RuleMetadata> TaggedFirewallController::GetMyRules() {
    auto allRules = EnumerateAllRules();
    std::vector<RuleMetadata> result;
    for (const auto& rule : allRules) {
        if (rule.ownerApp == identity_.appName && rule.ownerInstance == identity_.instanceId) {
            result.push_back(rule);
        }
    }
    return result;
}

std::vector<TaggedFirewallController::RuleMetadata> TaggedFirewallController::GetOurRules() {
    auto allRules = EnumerateAllRules();
    std::vector<RuleMetadata> result;
    for (const auto& rule : allRules) {
        if (rule.ownerApp == identity_.appName) {
            result.push_back(rule);
        }
    }
    return result;
}

std::vector<TaggedFirewallController::RuleMetadata>
TaggedFirewallController::GetRulesForProgram(const std::wstring& programPath) {
    auto allRules = EnumerateAllRules();
    std::vector<RuleMetadata> result;
    for (const auto& rule : allRules) {
        if (_wcsicmp(rule.targetProgram.c_str(), programPath.c_str()) == 0) {
            result.push_back(rule);
        }
    }
    return result;
}

std::vector<TaggedFirewallController::RuleMetadata>
TaggedFirewallController::GetRulesByType(const std::wstring& ruleType) {
    auto allRules = EnumerateAllRules();
    std::vector<RuleMetadata> result;
    for (const auto& rule : allRules) {
        if (rule.ruleType == ruleType) {
            result.push_back(rule);
        }
    }
    return result;
}

std::wstring TaggedFirewallController::BlockAllAccess(const std::wstring& programPath) {
    RuleMetadata meta;
    meta.ruleName = GenerateRuleName(programPath, L"ALL");
    meta.ownerApp = identity_.appName;
    meta.ownerInstance = identity_.instanceId;
    meta.ruleType = L"ALL";
    meta.createdTimestamp = GetCurrentTimestamp();
    meta.targetProgram = programPath;
    meta.remoteAddresses = L"*";
    meta.direction = Direction::OUTBOUND;
    meta.protocol = Protocol::ANY;
    meta.enabled = true;

    if (AddRule(meta)) {
        return meta.ruleName;
    }
    return L"";
}

std::wstring TaggedFirewallController::BlockIpAccess(const std::wstring& programPath,
    const std::wstring& ipAddress, Direction dir) {

    RuleMetadata meta;
    meta.ruleName = GenerateRuleName(programPath, ipAddress);
    meta.ownerApp = identity_.appName;
    meta.ownerInstance = identity_.instanceId;
    meta.ruleType = L"IP";
    meta.createdTimestamp = GetCurrentTimestamp();
    meta.targetProgram = programPath;
    meta.remoteAddresses = ipAddress;
    meta.direction = dir;
    meta.protocol = Protocol::ANY;
    meta.enabled = true;

    if (AddRule(meta)) {
        return meta.ruleName;
    }
    return L"";
}

std::wstring TaggedFirewallController::BlockDomainAccess(const std::wstring& programPath,
    const std::wstring& domainName, Direction dir) {

    auto ips = ResolveDomain(domainName);
    if (ips.empty()) {
        return L"";
    }

    std::wstring baseName = GenerateRuleName(programPath, domainName);

    DomainRuleEntry entry;
    entry.domain = domainName;
    entry.programPath = programPath;
    entry.baseRuleName = baseName;
    entry.direction = dir;
    entry.currentIps = ips;

    EnterCriticalSection(&domainCs_);
    domainRules_.push_back(entry);
    LeaveCriticalSection(&domainCs_);

    bool success = true;
    for (size_t i = 0; i < ips.size(); ++i) {
        RuleMetadata meta;
        meta.ruleName = baseName + L"_" + std::to_wstring(i);
        meta.ownerApp = identity_.appName;
        meta.ownerInstance = identity_.instanceId;
        meta.ruleType = L"DOMAIN";
        meta.createdTimestamp = GetCurrentTimestamp();
        meta.targetProgram = programPath;
        meta.targetDomain = domainName;
        meta.remoteAddresses = ips[i];
        meta.direction = dir;
        meta.protocol = Protocol::ANY;
        meta.enabled = true;

        if (!AddRule(meta)) {
            success = false;
        }
    }

    return success ? baseName : L"";
}

std::wstring TaggedFirewallController::BlockPortAccess(const std::wstring& programPath, UINT16 port,
    Protocol proto, Direction dir) {
    return BlockPortRange(programPath, port, port, proto, dir);
}

std::wstring TaggedFirewallController::BlockPortRange(const std::wstring& programPath,
    UINT16 startPort, UINT16 endPort, Protocol proto, Direction dir) {

    std::wstring portStr = (startPort == endPort) ?
        std::to_wstring(startPort) :
        std::to_wstring(startPort) + L"-" + std::to_wstring(endPort);

    RuleMetadata meta;
    std::wstring targetSpecifier = L"PORT:" + portStr + L"/" + ProtocolToString(proto);
    meta.ruleName = GenerateRuleName(programPath, targetSpecifier);
    meta.ownerApp = identity_.appName;
    meta.ownerInstance = identity_.instanceId;
    meta.ruleType = L"PORT";
    meta.createdTimestamp = GetCurrentTimestamp();
    meta.targetProgram = programPath;
    meta.remotePorts = portStr;
    meta.direction = dir;
    meta.protocol = proto;
    meta.enabled = true;

    if (AddRule(meta)) {
        return meta.ruleName;
    }
    return L"";
}

bool TaggedFirewallController::DeleteMyRule(const std::wstring& ruleName) {
    if (!IsMyRule(ruleName)) return false;
    return RemoveRule(ruleName);
}

bool TaggedFirewallController::DeleteAllMyRules() {
    auto myRules = GetMyRules();
    bool success = true;
    for (const auto& rule : myRules) {
        if (!RemoveRule(rule.ruleName)) success = false;
    }
    return success;
}

bool TaggedFirewallController::DeleteAllOurRules() {
    auto ourRules = GetOurRules();
    bool success = true;
    for (const auto& rule : ourRules) {
        if (!RemoveRule(rule.ruleName)) success = false;
    }
    return success;
}

bool TaggedFirewallController::DeleteRulesForProgram(const std::wstring& programPath) {
    auto progRules = GetRulesForProgram(programPath);
    bool success = true;
    for (const auto& rule : progRules) {
        if (rule.ownerApp == identity_.appName && !RemoveRule(rule.ruleName))
            success = false;
    }
    return success;
}

bool TaggedFirewallController::EnableMyRule(const std::wstring& ruleName, bool enable) {
    if (!IsMyRule(ruleName)) return false;
    return SetRuleEnabled(ruleName, enable);
}

bool TaggedFirewallController::UpdateMyRule(const std::wstring& ruleName, const RuleMetadata& newMetadata) {
    if (!IsMyRule(ruleName)) return false;
    if (!RemoveRule(ruleName)) return false;

    RuleMetadata meta = newMetadata;
    meta.ruleName = ruleName;
    meta.ownerApp = identity_.appName;
    meta.ownerInstance = identity_.instanceId;
    return AddRule(meta);
}

bool TaggedFirewallController::IsMyRule(const std::wstring& ruleName) {
    RuleMetadata meta;
    if (!ParseRuleName(ruleName, meta)) return false;
    return meta.ownerApp == identity_.appName && meta.ownerInstance == identity_.instanceId;
}

bool TaggedFirewallController::IsOurRule(const std::wstring& ruleName) {
    RuleMetadata meta;
    if (!ParseRuleName(ruleName, meta)) return false;
    return meta.ownerApp == identity_.appName;
}

bool TaggedFirewallController::GetRuleMetadata(const std::wstring& ruleName, RuleMetadata& outMeta) {
    if (!rules_) return false;

    INetFwRule* rule = nullptr;
    BSTR bstrName = SysAllocString(ruleName.c_str());
    HRESULT hr = rules_->Item(bstrName, &rule);
    SysFreeString(bstrName);

    if (FAILED(hr)) return false;

    bool result = ExtractMetadataFromRule(rule, outMeta);
    rule->Release();
    return result;
}

bool TaggedFirewallController::CleanupOrphanedRules(const std::wstring& appName) {
    auto allRules = EnumerateAllRules();
    bool success = true;
    for (const auto& rule : allRules) {
        if (!rule.IsValid()) continue;
        if (!appName.empty() && rule.ownerApp != appName) continue;

        DWORD attribs = GetFileAttributesW(rule.targetProgram.c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES) {
            if (!RemoveRule(rule.ruleName)) success = false;
        }
    }
    return success;
}

TaggedFirewallController::Statistics TaggedFirewallController::GetStatistics() {
    Statistics stats = {};
    auto myRules = GetMyRules();
    auto ourRules = GetOurRules();
    auto allRules = EnumerateAllRules();

    stats.totalMyRules = myRules.size();
    stats.totalOurRules = ourRules.size();
    stats.totalOtherRules = allRules.size() - ourRules.size();

    for (const auto& rule : myRules) {
        if (rule.enabled) stats.enabledRules++;
        else stats.disabledRules++;
    }
    return stats;
}

// ==================== 域名监控 ====================

void TaggedFirewallController::StartDomainMonitor(int intervalSeconds) {
    if (monitorRunning_) return;
    monitorInterval_ = intervalSeconds;
    monitorRunning_ = true;
    monitorThread_ = CreateThread(NULL, 0, DomainMonitorThread, this, 0, NULL);
}

void TaggedFirewallController::StopDomainMonitor() {
    if (!monitorRunning_) return;
    monitorRunning_ = false;
    if (monitorThread_) {
        WaitForSingleObject(monitorThread_, 5000);
        CloseHandle(monitorThread_);
        monitorThread_ = NULL;
    }
}

bool TaggedFirewallController::RefreshDomainRules() {
    EnterCriticalSection(&domainCs_);
    bool success = true;

    for (auto& entry : domainRules_) {
        auto newIps = ResolveDomain(entry.domain);
        if (newIps.empty()) continue;

        bool changed = (newIps.size() != entry.currentIps.size());
        if (!changed) {
            for (size_t i = 0; i < newIps.size() && i < entry.currentIps.size(); ++i) {
                if (newIps[i] != entry.currentIps[i]) { changed = true; break; }
            }
        }

        if (changed) {
            for (size_t i = 0; i < entry.currentIps.size(); ++i)
                RemoveRule(entry.baseRuleName + L"_" + std::to_wstring(i));

            for (size_t i = 0; i < newIps.size(); ++i) {
                RuleMetadata meta;
                meta.ruleName = entry.baseRuleName + L"_" + std::to_wstring(i);
                meta.ownerApp = identity_.appName;
                meta.ownerInstance = identity_.instanceId;
                meta.ruleType = L"DOMAIN";
                meta.createdTimestamp = GetCurrentTimestamp();
                meta.targetProgram = entry.programPath;
                meta.targetDomain = entry.domain;
                meta.remoteAddresses = newIps[i];
                meta.direction = entry.direction;
                meta.protocol = Protocol::ANY;
                meta.enabled = true;
                if (!AddRule(meta)) success = false;
            }
            entry.currentIps = newIps;
        }
    }

    LeaveCriticalSection(&domainCs_);
    return success;
}

DWORD WINAPI TaggedFirewallController::DomainMonitorThread(LPVOID param) {
    auto* controller = static_cast<TaggedFirewallController*>(param);
    controller->MonitorLoop();
    return 0;
}

void TaggedFirewallController::MonitorLoop() {
    while (monitorRunning_) {
        Sleep(monitorInterval_ * 1000);
        if (!monitorRunning_) break;
        RefreshDomainRules();
    }
}

std::wstring TaggedFirewallController::ComputeMD5(const std::wstring input) {
	MD5 ctx;
	ctx.add((const unsigned char*)input.c_str(), input.size() * sizeof(wchar_t));
    return WinUtils::AnsiToWide(ctx.getHash().c_str());
}

std::wstring TaggedFirewallController::ComputeTargetMd5(const std::wstring& programPath, const std::wstring& targetSpecifier) {
    std::wstring hashInput = programPath + L"|" + targetSpecifier;
    std::wstring md5 = ComputeMD5(hashInput);
    if (md5.length() > 16) {
        md5 = md5.substr(0, 16);
    }
    return md5;
}

bool TaggedFirewallController::ExtractMd5FromRuleName(const std::wstring& ruleName, std::wstring& outMd5) {
    if (ruleName.find(NAME_PREFIX) != 0) return false;
    std::wstring content = ruleName.substr(wcslen(NAME_PREFIX));
    size_t firstUnderscore = content.find(L'_');
    if (firstUnderscore == std::wstring::npos) return false;
    size_t secondUnderscore = content.find(L'_', firstUnderscore + 1);
    if (secondUnderscore == std::wstring::npos) return false;
    std::wstring rest = content.substr(secondUnderscore + 1);
    size_t suffixPos = rest.find(L'_');
    std::wstring md5 = (suffixPos == std::wstring::npos) ? rest : rest.substr(0, suffixPos);
    if (md5.length() != 16) return false;
    for (wchar_t c : md5) {
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'))) {
            return false;
        }
    }
    outMd5 = md5;
    return true;
}

bool TaggedFirewallController::DeleteRulesByTarget(const std::wstring& programPath, const std::wstring& targetSpecifier) {
    std::wstring targetMd5 = ComputeTargetMd5(programPath, targetSpecifier);
    bool success = true;
    auto ourRules = GetOurRules();
    for (const auto& rule : ourRules) {
        std::wstring md5;
        if (ExtractMd5FromRuleName(rule.ruleName, md5) && md5 == targetMd5) {
            if (!RemoveRule(rule.ruleName)) success = false;
        }
    }
    return success;
}