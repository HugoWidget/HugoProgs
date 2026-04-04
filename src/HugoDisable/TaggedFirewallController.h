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
#ifndef TAGGED_FIREWALL_CONTROLLER_H
#define TAGGED_FIREWALL_CONTROLLER_H
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <rpc.h> 
#include <netfw.h>
#include <objbase.h>
#include <oleauto.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>

#include "hashlib/md5.h"
#include "WinUtils/StrConvert.h"

class TaggedFirewallController {
public:
    struct ControllerIdentity {
        std::wstring appName;
        std::wstring instanceId;
        std::wstring version;
        ULONGLONG createdTime;
        ControllerIdentity() : createdTime(0) {}
    };

    enum class Direction {
        INBOUND = NET_FW_RULE_DIR_IN,
        OUTBOUND = NET_FW_RULE_DIR_OUT,
        BOTH = 3
    };

    enum class Protocol {
        ANY = NET_FW_IP_PROTOCOL_ANY,
        TCP = NET_FW_IP_PROTOCOL_TCP,
        UDP = NET_FW_IP_PROTOCOL_UDP
     //   ICMP = NET_FW_IP_PROTOCOL_ICMPv4
    };

    struct RuleMetadata {
        std::wstring ruleName;
        std::wstring ownerApp;
        std::wstring ownerInstance;
        std::wstring ruleType;
        ULONGLONG createdTimestamp;
        std::wstring targetProgram;
        std::wstring targetDomain;
        std::wstring remoteAddresses;
        std::wstring remotePorts;
        Protocol protocol;
        Direction direction;
        bool enabled;

        bool IsValid() const { return !ownerApp.empty(); }
    };

    struct Statistics {
        size_t totalMyRules = 0, totalOurRules = 0, totalOtherRules = 0;
        size_t enabledRules = 0, disabledRules = 0;
    };

    explicit TaggedFirewallController(const ControllerIdentity& identity);
    ~TaggedFirewallController();

    static TaggedFirewallController& GetDefaultInstance();

    bool Initialize();
    void Shutdown();

    TaggedFirewallController(const TaggedFirewallController&) = delete;
    TaggedFirewallController& operator=(const TaggedFirewallController&) = delete;

    std::wstring BlockAllAccess(const std::wstring& programPath);
    std::wstring BlockIpAccess(const std::wstring& programPath, const std::wstring& ipAddress,
        Direction dir = Direction::OUTBOUND);
    std::wstring BlockDomainAccess(const std::wstring& programPath, const std::wstring& domainName,
        Direction dir = Direction::OUTBOUND);
    std::wstring BlockPortAccess(const std::wstring& programPath, UINT16 port,
        Protocol proto = Protocol::TCP, Direction dir = Direction::OUTBOUND);
    std::wstring BlockPortRange(const std::wstring& programPath, UINT16 startPort, UINT16 endPort,
        Protocol proto = Protocol::TCP, Direction dir = Direction::OUTBOUND);

    std::vector<RuleMetadata> GetMyRules();
    std::vector<RuleMetadata> GetOurRules();
    std::vector<RuleMetadata> GetRulesForProgram(const std::wstring& programPath);
    std::vector<RuleMetadata> GetRulesByType(const std::wstring& ruleType);
    std::vector<RuleMetadata> GetAllRules();

    bool DeleteMyRule(const std::wstring& ruleName);
    bool DeleteAllMyRules();
    bool DeleteAllOurRules();
    bool DeleteRulesForProgram(const std::wstring& programPath);
    bool EnableMyRule(const std::wstring& ruleName, bool enable);
    bool UpdateMyRule(const std::wstring& ruleName, const RuleMetadata& newMetadata);

    bool IsMyRule(const std::wstring& ruleName);
    bool IsOurRule(const std::wstring& ruleName);
    bool GetRuleMetadata(const std::wstring& ruleName, RuleMetadata& outMeta);
    bool CleanupOrphanedRules(const std::wstring& appName = L"");
    Statistics GetStatistics();

    void StartDomainMonitor(int intervalSeconds = 300);
    void StopDomainMonitor();
    bool RefreshDomainRules();

    bool EnableFirewall();
    bool DisableFirewall();
    bool SetDefaultInboundAction(bool allow);
    bool SetDefaultOutboundAction(bool allow);
    bool IsFirewallEnabled();
    bool CleanupResidualRules();

    bool DeleteRulesByTarget(const std::wstring& programPath, const std::wstring& targetSpecifier);

private:
    ControllerIdentity identity_;
    bool initialized_;
    bool comInitialized_;
    INetFwPolicy2* policy_;
    INetFwRules* rules_;
    HANDLE monitorThread_;
    bool monitorRunning_;
    int monitorInterval_;
    CRITICAL_SECTION domainCs_;

    struct DomainRuleEntry {
        std::wstring domain, programPath, baseRuleName;
        Direction direction;
        std::vector<std::wstring> currentIps;
    };
    std::vector<DomainRuleEntry> domainRules_;

    static constexpr const wchar_t* NAME_PREFIX = L"NC_";

    std::wstring GenerateRuleName(const std::wstring& programPath, const std::wstring& targetSpecifier);
    bool ParseRuleName(const std::wstring& ruleName, RuleMetadata& outMeta);
    std::wstring SanitizeName(const std::wstring& input);


    bool AddRule(const RuleMetadata& meta);
    bool RemoveRule(const std::wstring& ruleName);
    bool SetRuleEnabled(const std::wstring& ruleName, bool enabled);

    std::vector<RuleMetadata> EnumerateAllRules();
    bool ExtractMetadataFromRule(INetFwRule* rule, RuleMetadata& outMeta);

    std::wstring ProtocolToString(Protocol p);
    std::wstring DirectionToString(Direction d);
    Protocol IntToProtocol(LONG proto);
    Direction IntToDirection(NET_FW_RULE_DIRECTION dir);

    std::vector<std::wstring> ResolveDomain(const std::wstring& domain);
    ULONGLONG GetCurrentTimestamp();
    bool InitializeFirewallPolicy();

    static DWORD WINAPI DomainMonitorThread(LPVOID param);
    void MonitorLoop();

	std::wstring ComputeMD5(const std::wstring input);
    std::wstring ComputeTargetMd5(const std::wstring& programPath, const std::wstring& targetSpecifier);
    bool ExtractMd5FromRuleName(const std::wstring& ruleName, std::wstring& outMd5);
};

#endif

