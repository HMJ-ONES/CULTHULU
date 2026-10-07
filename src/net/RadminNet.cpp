#include "net/RadminNet.h"

#include "core/Logger.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

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
