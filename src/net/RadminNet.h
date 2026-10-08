#pragma once

// Radmin VPN adapter detection for CULT-ULHU multiplayer.
//
// Radmin VPN gives every machine a virtual-LAN address in 26.0.0.0/8.
// The host's game is reachable at that address by everyone in the same
// Radmin network — no port forwarding, no dedicated server.
//
// If no Radmin adapter is present we fall back gracefully (loopback for
// local testing, or the machine's LAN address) and say so in the log.

#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace cultulhu {
namespace net {

struct NetAdapter {
    std::string name;     // e.g. "radmin0", "eth0", "lo"
    std::string ip;       // dotted quad, IPv4 only
    std::string netmask;  // dotted quad

    bool isRadmin() const;    // 26.0.0.0/8
    bool isLoopback() const;  // 127.0.0.0/8
};

// Enumerate IPv4 adapters: getifaddrs() on POSIX, GetAdaptersAddresses()
// (<iphlpapi.h>) on Windows. Test seam below needs no OS at all.
std::vector<NetAdapter> listAdapters();

// Test seam: classify raw (name, ip, netmask) tuples without touching the OS.
std::vector<NetAdapter> classifyAdapters(
    const std::vector<std::tuple<std::string, std::string, std::string>>& raw);

std::optional<NetAdapter> findRadminAdapter(const std::vector<NetAdapter>&);

// "192.168.1.20" + "255.255.255.0" -> "192.168.1.255".
std::string broadcastAddress(const NetAdapter& a);

// Convenience: Radmin adapter if present, else first non-loopback IPv4,
// else loopback. Always logs which one was picked and why.
NetAdapter preferredAdapter(const std::vector<NetAdapter>& adapters);

} // namespace net
} // namespace cultulhu
