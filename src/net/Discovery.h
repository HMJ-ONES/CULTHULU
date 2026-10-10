#pragma once

// UDP lobby discovery over the Radmin virtual LAN.
//
// The host broadcasts a presence beacon every 2 seconds to the subnet
// broadcast address; clients listen and build a list of joinable games.
// No central server involved — pure virtual-LAN multicast-style discovery.

#include "net/Socket.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cultulhu {
namespace net {

constexpr uint16_t kDiscoveryPort = 47777;
constexpr double kBeaconIntervalSec = 2.0;
constexpr double kHostExpirySec = 6.0;

struct DiscoveredHost {
    std::string ip;
    std::string hostName;
    std::string mode;
    int players = 0;
    int maxPlayers = 10;
    uint16_t tcpPort = 0;
    double lastSeen = 0.0;
};

// Beacon wire format: "CULTHULU|1|<hostName>|<mode>|<players>|<max>|<tcpPort>"
std::string formatBeacon(const std::string& hostName, const std::string& mode,
                         int players, int maxPlayers, uint16_t tcpPort);
bool parseBeacon(const std::string& text, const std::string& fromIp,
                 double now, DiscoveredHost& out);

class HostBeacon {
public:
    HostBeacon(std::string hostName, std::string mode, uint16_t tcpPort,
               int maxPlayers = 10);

    // targetIp: subnet broadcast (e.g. 26.x broadcast). For loopback tests
    // pass "127.0.0.1" via setTargetOverride().
    bool start(const std::string& broadcastIp);
    void setTargetOverride(const std::string& ip) { targetOverride_ = ip; }
    void setPlayerCount(int n) { players_ = n; }
    void tick(double nowSeconds);  // call often; sends every kBeaconIntervalSec
    void stop();

private:
    std::string hostName_, mode_, broadcastIp_, targetOverride_;
    uint16_t tcpPort_;
    int players_ = 0;
    int maxPlayers_;
    double lastSent_ = -1e9;
    UdpSocket sock_;
};

class DiscoveryClient {
public:
    bool start();  // binds kDiscoveryPort
    // Blocking collect for up to timeoutMs; prunes stale entries.
    std::vector<DiscoveredHost> listenFor(int timeoutMs);
    // Single non-blocking pass; keeps internal list across calls.
    std::vector<DiscoveredHost> poll();
    const std::vector<DiscoveredHost>& hosts() const { return hosts_; }
    void stop();

private:
    UdpSocket sock_;
    std::vector<DiscoveredHost> hosts_;
};

} // namespace net
} // namespace cultulhu
