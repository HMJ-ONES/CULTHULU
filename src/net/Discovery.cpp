#include "net/Discovery.h"

#include "core/Logger.h"
#include "net/Socket.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace cultulhu {
namespace net {

std::string formatBeacon(const std::string& hostName, const std::string& mode,
                         int players, int maxPlayers, uint16_t tcpPort) {
    return "CULTHULU|1|" + hostName + "|" + mode + "|" +
           std::to_string(players) + "|" + std::to_string(maxPlayers) + "|" +
           std::to_string(tcpPort);
}

bool parseBeacon(const std::string& text, const std::string& fromIp,
                 double now, DiscoveredHost& out) {
    // Split on '|'; expect 7 parts with magic + version.
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= text.size()) {
        size_t bar = text.find('|', i);
        parts.push_back(text.substr(i, bar == std::string::npos ? bar
                                                                : bar - i));
        if (bar == std::string::npos) break;
        i = bar + 1;
    }
    if (parts.size() != 7 || parts[0] != "CULTHULU" || parts[1] != "1")
        return false;
    // Sanitize '|' out of names (they would break the format).
    for (char& c : parts[2])
        if (c == '|') c = ' ';
    for (char& c : parts[3])
        if (c == '|') c = ' ';
    try {
        out.ip = fromIp;
        out.hostName = parts[2].empty() ? fromIp : parts[2];
        out.mode = parts[3];
        out.players = std::stoi(parts[4]);
        out.maxPlayers = std::stoi(parts[5]);
        out.tcpPort = static_cast<uint16_t>(std::stoi(parts[6]));
    } catch (...) {
        return false;
    }
    out.lastSeen = now;
    return true;
}

// ---------------- HostBeacon ----------------

HostBeacon::HostBeacon(std::string hostName, std::string mode,
                       uint16_t tcpPort, int maxPlayers)
    : hostName_(std::move(hostName)),
      mode_(std::move(mode)),
      tcpPort_(tcpPort),
      maxPlayers_(maxPlayers) {}

bool HostBeacon::start(const std::string& broadcastIp) {
    broadcastIp_ = broadcastIp;
    if (!sock_.open() || !sock_.setBroadcast(true)) {
        Logger::error("HostBeacon: failed to open broadcast socket");
        return false;
    }
    Logger::info("HostBeacon: broadcasting '" + hostName_ + "' to " +
                 broadcastIp + ":" + std::to_string(kDiscoveryPort));
    return true;
}

void HostBeacon::tick(double now) {
    if (!sock_.valid()) return;
    if (now - lastSent_ < kBeaconIntervalSec) return;
    lastSent_ = now;
    std::string target = targetOverride_.empty() ? broadcastIp_ : targetOverride_;
    std::string pkt =
        formatBeacon(hostName_, mode_, players_, maxPlayers_, tcpPort_);
    sock_.sendTo(target, kDiscoveryPort, pkt.data(), pkt.size());
}

void HostBeacon::stop() { sock_.close(); }

// ---------------- DiscoveryClient ----------------

bool DiscoveryClient::start() {
    if (!sock_.bind(kDiscoveryPort)) {
        Logger::error("DiscoveryClient: cannot bind UDP " +
                      std::to_string(kDiscoveryPort));
        return false;
    }
    return true;
}

std::vector<DiscoveredHost> DiscoveryClient::poll() {
    double now = nowSeconds();
    char buf[1024];
    for (;;) {
        std::string fromIp;
        uint16_t fromPort = 0;
        long n = sock_.recvFrom(buf, sizeof(buf) - 1, fromIp, fromPort, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        DiscoveredHost h;
        if (!parseBeacon(buf, fromIp, now, h)) continue;
        bool known = false;
        for (auto& e : hosts_) {
            if (e.ip == h.ip && e.tcpPort == h.tcpPort) {
                e = h;
                known = true;
                break;
            }
        }
        if (!known) {
            Logger::info("Discovered host '" + h.hostName + "' at " + h.ip +
                         ":" + std::to_string(h.tcpPort) + " (" +
                         std::to_string(h.players) + "/" +
                         std::to_string(h.maxPlayers) + ")");
            hosts_.push_back(h);
        }
    }
    // Prune stale.
    hosts_.erase(
        std::remove_if(hosts_.begin(), hosts_.end(),
                       [now](const DiscoveredHost& h) {
                           return now - h.lastSeen > kHostExpirySec;
                       }),
        hosts_.end());
    return hosts_;
}

std::vector<DiscoveredHost> DiscoveryClient::listenFor(int timeoutMs) {
    double deadline = nowSeconds() + timeoutMs / 1000.0;
    while (nowSeconds() < deadline) {
        poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return poll();
}

void DiscoveryClient::stop() {
    sock_.close();
    hosts_.clear();
}

} // namespace net
} // namespace cultulhu
