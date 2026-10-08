#include "net/RadminNet.h"

#include "core/Logger.h"

#ifdef _WIN32
// Windows adapter enumeration: GetAdaptersAddresses() from <iphlpapi.h>.
// There is no getifaddrs() on Windows. Unicast addresses carry a CIDR
// prefix length (OnLinkPrefixLength) instead of a netmask, so we convert
// it to a dotted quad to keep the shared NetAdapter math unchanged.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#endif

#include <cstdint>
#include <sstream>

namespace cultulhu {
namespace net {
namespace {

uint32_t quadToInt(const std::string& q) {
    in_addr a{};
    if (inet_pton(AF_INET, q.c_str(), &a) != 1) return 0;
    return ntohl(a.s_addr);
}

std::string intToQuad(uint32_t v) {
    in_addr a{};
    a.s_addr = htonl(v);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return buf;
}

bool inSubnet(const std::string& ip, uint32_t net, int bits) {
    return (quadToInt(ip) >> (32 - bits)) == (net >> (32 - bits));
}

#ifdef _WIN32
// Adapter FriendlyName is WCHAR; convert for log parity with ifa_name.
std::string wideToUtf8(const wchar_t* w) {
    if (!w || !*w) return "?";
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr,
                                nullptr);
    if (n <= 1) return "?";
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
#endif

} // namespace

bool NetAdapter::isRadmin() const { return inSubnet(ip, 0x1A000000, 8); }  // 26/8
bool NetAdapter::isLoopback() const { return inSubnet(ip, 0x7F000000, 8); }  // 127/8

std::vector<NetAdapter> classifyAdapters(
    const std::vector<std::tuple<std::string, std::string, std::string>>& raw) {
    std::vector<NetAdapter> out;
    for (const auto& [name, ip, mask] : raw) {
        if (ip.empty() || quadToInt(ip) == 0) continue;
        out.push_back(NetAdapter{name, ip, mask.empty() ? "255.255.255.0" : mask});
    }
    return out;
}

std::vector<NetAdapter> listAdapters() {
    std::vector<std::tuple<std::string, std::string, std::string>> raw;
#ifdef _WIN32
    // GetAdaptersAddresses needs a sized buffer; the documented pattern is
    // to retry with the size it reports via ERROR_BUFFER_OVERFLOW.
    ULONG bufLen = 15 * 1024;
    IP_ADAPTER_ADDRESSES* addrs = nullptr;
    DWORD rc = ERROR_BUFFER_OVERFLOW;
    for (int tries = 0; tries < 3 && rc == ERROR_BUFFER_OVERFLOW; ++tries) {
        delete[] reinterpret_cast<char*>(addrs);
        addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(new char[bufLen]);
        rc = GetAdaptersAddresses(AF_INET,
                                  GAA_FLAG_SKIP_ANYCAST |
                                      GAA_FLAG_SKIP_MULTICAST |
                                      GAA_FLAG_SKIP_DNS_SERVER,
                                  nullptr, addrs, &bufLen);
    }
    if (rc != NO_ERROR || !addrs) {
        Logger::warn("GetAdaptersAddresses failed; no adapters enumerated");
        delete[] reinterpret_cast<char*>(addrs);
        return {};
    }
    for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        const std::string name = wideToUtf8(a->FriendlyName);
        for (IP_ADAPTER_UNICAST_ADDRESS* ua = a->FirstUnicastAddress; ua;
             ua = ua->Next) {
            if (!ua->Address.lpSockaddr ||
                ua->Address.lpSockaddr->sa_family != AF_INET)
                continue;
            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            char ipbuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sin->sin_addr, ipbuf, sizeof(ipbuf));
            // CIDR prefix length -> dotted netmask for the shared math.
            const ULONG bits = ua->OnLinkPrefixLength;
            const uint32_t mask = (bits >= 32)   ? 0xFFFFFFFFu
                                  : (bits == 0) ? 0x00000000u
                                                : (0xFFFFFFFFu << (32 - bits));
            raw.emplace_back(name, ipbuf, intToQuad(mask));
        }
    }
    delete[] reinterpret_cast<char*>(addrs);
#else
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) {
        Logger::warn("getifaddrs failed; no adapters enumerated");
        return {};
    }
    for (ifaddrs* it = list; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
        if (!it->ifa_netmask || it->ifa_netmask->sa_family != AF_INET) continue;
        auto* sin = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
        auto* mask = reinterpret_cast<sockaddr_in*>(it->ifa_netmask);
        char ipbuf[INET_ADDRSTRLEN], maskbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, ipbuf, sizeof(ipbuf));
        inet_ntop(AF_INET, &mask->sin_addr, maskbuf, sizeof(maskbuf));
        raw.emplace_back(it->ifa_name ? it->ifa_name : "?",
                         ipbuf, maskbuf);
    }
    freeifaddrs(list);
#endif
    return classifyAdapters(raw);
}

std::optional<NetAdapter> findRadminAdapter(const std::vector<NetAdapter>& as) {
    for (const auto& a : as)
        if (a.isRadmin()) return a;
    return std::nullopt;
}

std::string broadcastAddress(const NetAdapter& a) {
    uint32_t bcast = quadToInt(a.ip) | ~quadToInt(a.netmask);
    return intToQuad(bcast);
}

NetAdapter preferredAdapter(const std::vector<NetAdapter>& adapters) {
    if (auto r = findRadminAdapter(adapters)) {
        Logger::info("Using Radmin VPN adapter " + r->name + " (" + r->ip +
                     "); friends join this IP.");
        return *r;
    }
    for (const auto& a : adapters) {
        if (!a.isLoopback()) {
            Logger::warn("No Radmin VPN adapter found (no 26.x.x.x address). "
                         "Falling back to LAN adapter " + a.name + " (" + a.ip +
                         "). Install Radmin VPN and join the network for "
                         "internet play.");
            return a;
        }
    }
    Logger::warn("No Radmin VPN adapter and no LAN adapter found. Falling "
                 "back to loopback (127.0.0.1) — local testing only.");
    return NetAdapter{"lo", "127.0.0.1", "255.0.0.0"};
}

} // namespace net
} // namespace cultulhu
