// CULT-ULHU wave 6 tests: Radmin VPN multiplayer netcode.
// Covers: protocol framing round-trips, partial TCP reassembly,
// Radmin adapter detection (mocked interfaces), beacon format/parse,
// loopback discovery, lobby handshake, and host-authoritative
// input/snapshot flow over loopback.

#include "net/Discovery.h"
#include "net/Lobby.h"
#include "net/Netcode.h"
#include "net/Protocol.h"
#include "net/RadminNet.h"
#include "net/Socket.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

using namespace cultulhu;
using namespace cultulhu::net;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

#define CHECK_CLOSE(a, b, eps) do { ++checks; if (std::fabs((a) - (b)) > (eps)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #a " (" << (a) \
    << ") != " #b " (" << (b) << ")\n"; } } while (0)

static void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Pump both sides of a connection for ~ms milliseconds.
template <typename A, typename B>
static void pump(A& a, B& b, int ms) {
    double end = nowSeconds() + ms / 1000.0;
    while (nowSeconds() < end) {
        a.poll();
        b.poll();
        sleepMs(5);
    }
}

// ---------------- Protocol ----------------

static void testProtocolRoundTrip() {
    for (int t = 0; t <= static_cast<int>(MsgType::Disconnect); ++t) {
        Message m{static_cast<MsgType>(t),
                  {{"name", "Alice"}, {"id", "3"}, {"text", "hi;drop=me"}}};
        auto bytes = encode(m);
        auto back = decodeFrame(bytes.data(), bytes.size());
        CHECK(back.has_value());
        CHECK(back->type == m.type);
        CHECK(back->fields["name"] == "Alice");
        CHECK(back->fields["id"] == "3");
        // ';' and '=' are sanitized out of chat text.
        CHECK(back->fields["text"] == "hidropme");
    }
    // Short buffer -> no frame.
    uint8_t tiny[3] = {0, 0, 0};
    CHECK(!decodeFrame(tiny, 3).has_value());
}

static void testMessageReaderPartial() {
    Message m{MsgType::ChatMsg, {{"from", "Bob"}, {"text", "hello"}}};
    auto bytes = encode(m);
    MessageReader r;
    // Feed in awkward chunks.
    r.feed(bytes.data(), 2);
    CHECK(r.drain().empty());
    r.feed(bytes.data() + 2, 3);
    CHECK(r.drain().empty());
    r.feed(bytes.data() + 5, bytes.size() - 5);
    auto out = r.drain();
    CHECK(out.size() == 1);
    CHECK(out[0].type == MsgType::ChatMsg);
    CHECK(fieldStr(out[0], "from") == "Bob");
    // Two messages back to back.
    MessageReader r2;
    auto b2 = encode(m);
    std::vector<uint8_t> both = bytes;
    both.insert(both.end(), b2.begin(), b2.end());
    r2.feed(both.data(), both.size());
    CHECK(r2.drain().size() == 2);
}

// ---------------- RadminNet ----------------

static void testRadminClassify() {
    auto as = classifyAdapters({
        {"lo", "127.0.0.1", "255.0.0.0"},
        {"eth0", "192.168.1.20", "255.255.255.0"},
        {"radmin0", "26.123.45.67", "255.0.0.0"},
    });
    CHECK(as.size() == 3);
    auto r = findRadminAdapter(as);
    CHECK(r.has_value());
    CHECK(r->ip == "26.123.45.67");
    CHECK(r->isRadmin());
    CHECK(!as[1].isRadmin());
    CHECK(as[0].isLoopback());

    // Broadcast: 26.123.45.67/8 -> 26.255.255.255
    CHECK(broadcastAddress(*r) == "26.255.255.255");
    CHECK(broadcastAddress(as[1]) == "192.168.1.255");

    // Preferred picks Radmin.
    CHECK(preferredAdapter(as).ip == "26.123.45.67");

    // No Radmin -> LAN fallback.
    auto noRadmin = classifyAdapters({
        {"lo", "127.0.0.1", "255.0.0.0"},
        {"eth0", "10.0.0.5", "255.255.255.0"},
    });
    CHECK(!findRadminAdapter(noRadmin).has_value());
    CHECK(preferredAdapter(noRadmin).ip == "10.0.0.5");

    // Nothing at all -> loopback.
    CHECK(preferredAdapter({}).ip == "127.0.0.1");
}

// ---------------- Discovery ----------------

static void testBeaconParse() {
    DiscoveredHost h;
    double now = 100.0;
    CHECK(parseBeacon("CULTHULU|1|EldritchHost|freeroam|3|10|47778",
                      "26.1.2.3", now, h));
    CHECK(h.ip == "26.1.2.3");
    CHECK(h.hostName == "EldritchHost");
    CHECK(h.mode == "freeroam");
    CHECK(h.players == 3);
    CHECK(h.maxPlayers == 10);
    CHECK(h.tcpPort == 47778);
    CHECK(h.lastSeen == now);
    // Bad magic / version rejected.
    CHECK(!parseBeacon("OTHER|1|x|y|1|10|5", "1.2.3.4", now, h));
    CHECK(!parseBeacon("CULTHULU|2|x|y|1|10|5", "1.2.3.4", now, h));
    CHECK(!parseBeacon("CULTHULU|1|only", "1.2.3.4", now, h));
}

static void testDiscoveryLoopback() {
    // Sandbox environments sometimes block UDP outright (EPERM on
    // sendto); the beacon format/parse tests above still validate the
    // protocol, and this live test runs on real machines.
    {
        UdpSocket probe;
        if (!probe.open() || !probe.sendTo("127.0.0.1", 47999, "x", 1)) {
            std::cout << "SKIP testDiscoveryLoopback (UDP blocked here)\n";
            return;
        }
    }
    HostBeacon beacon("TestHost", "freeroam", 47799, 10);
    CHECK(beacon.start("127.0.0.1"));
    beacon.setTargetOverride("127.0.0.1");
    beacon.setPlayerCount(2);

    DiscoveryClient dc;
    CHECK(dc.start());
    // Beacon immediately on first tick (lastSent_ starts far in past).
    beacon.tick(nowSeconds());
    auto hosts = dc.listenFor(1500);
    CHECK(!hosts.empty());
    bool found = false;
    for (const auto& h : hosts) {
        if (h.hostName == "TestHost" && h.tcpPort == 47799 &&
            h.players == 2) {
            found = true;
        }
    }
    CHECK(found);
    beacon.stop();
    dc.stop();
}

// ---------------- Lobby handshake ----------------

static void testLobbyHandshake() {
    HostLobby host(0, "Hosty");  // ephemeral port
    CHECK(host.start());
    uint16_t port = host.port();
    CHECK(port != 0);

    JoinLobby client;
    CHECK(client.connect("127.0.0.1", port, "Alice"));

    // Pump until the client is welcomed and sees the roster.
    bool welcomed = false;
    for (int i = 0; i < 200 && !welcomed; ++i) {
        host.poll();
        client.poll();
        auto ps = client.players();
        welcomed = ps.size() == 2;
        sleepMs(10);
    }
    CHECK(welcomed);
    auto ps = client.players();
    CHECK(ps[0].name == "Hosty");
    CHECK(ps[1].name == "Alice");
    CHECK(ps[1].team == 0 || ps[1].team == 1);

    // Ready-up flow.
    client.setReady(true);
    host.setHostReady(true);
    bool allReady = false;
    for (int i = 0; i < 200 && !allReady; ++i) {
        host.poll();
        client.poll();
        allReady = host.allReady();
        sleepMs(10);
    }
    CHECK(allReady);

    // Start.
    CHECK(host.startGame(false, "freeroam"));
    bool started = false;
    for (int i = 0; i < 200 && !started; ++i) {
        host.poll();
        client.poll();
        started = client.gameStarted();
        sleepMs(10);
    }
    CHECK(started);
    CHECK(client.startMode() == "freeroam");

    // Hand sockets to netcode (tested below in testNetcodeLoopback style).
    auto hsocks = host.takeClientSockets();
    CHECK(hsocks.size() == 1);
    auto csock = client.takeSocket();
    CHECK(csock.valid());
}

// ---------------- Netcode ----------------

static void testInputSnapshotCodec() {
    ClientInput in{42, 0.5f, -0.25f, 1.57f, 0b101};
    auto msg = encodeClientInput(in);
    ClientInput back;
    CHECK(decodeClientInput(msg, back));
    CHECK(back.seq == 42);
    CHECK_CLOSE(back.moveX, 0.5f, 0.001f);
    CHECK_CLOSE(back.moveZ, -0.25f, 0.001f);
    CHECK_CLOSE(back.yaw, 1.57f, 0.001f);
    CHECK(back.buttons == 0b101);

    std::vector<SnapshotEntity> ents = {
        {1, 10.0f, 0.0f, -5.0f, 100.0f, 3},
        {2, 0.0f, 1.0f, 2.0f, 55.5f, 0},
    };
    auto sm = encodeSnapshot(77, ents);
    uint32_t tick = 0;
    std::vector<SnapshotEntity> eb;
    CHECK(decodeSnapshot(sm, tick, eb));
    CHECK(tick == 77);
    CHECK(eb.size() == 2);
    CHECK(eb[0].id == 1);
    CHECK_CLOSE(eb[0].x, 10.0f, 0.01f);
    CHECK_CLOSE(eb[1].hp, 55.5f, 0.1f);
    CHECK(eb[0].state == 3);
}

static void testNetcodeLoopback() {
    // Set up a host + client pair directly over loopback TCP.
    TcpListener listener;
    CHECK(listener.listen(0));
    uint16_t port = listener.boundPort();

    TcpSocket raw;
    CHECK(raw.connect("127.0.0.1", port, 3000));
    auto acc = listener.accept(2000);
    CHECK(acc.has_value());

    std::vector<TcpSocket> hsocks;
    hsocks.push_back(std::move(*acc));
    NetHost nh(std::move(hsocks));
    NetClient nc(std::move(raw));

    // Run ~1.2s of host/client polling.
    double end = nowSeconds() + 1.2;
    std::vector<SnapshotEntity> world = {
        {1, 1.0f, 0.0f, 2.0f, 100.0f, 0},
        {7, 5.0f, 0.0f, 6.0f, 80.0f, 1},
    };
    while (nowSeconds() < end) {
        nh.poll(nowSeconds(), [&]() { return world; });
        nc.poll(nowSeconds(), []() {
            ClientInput in;
            in.moveX = 1.0f;
            in.buttons = 1;
            return in;
        });
        sleepMs(5);
    }

    // Host received client input.
    CHECK(!nh.latestInputs().empty());
    bool sawInput = false;
    for (const auto& [idx, in] : nh.latestInputs()) {
        if (in.buttons == 1 && in.moveX > 0.9f) sawInput = true;
    }
    CHECK(sawInput);

    // Client applied snapshots directly.
    const auto& ents = nc.entities();
    CHECK(ents.size() == 2);
    auto it = ents.find(1);
    CHECK(it != ents.end());
    CHECK_CLOSE(it->second.x, 1.0f, 0.01f);
    CHECK_CLOSE(it->second.hp, 100.0f, 0.1f);
    CHECK(nc.lastTick() > 0);
    CHECK(nc.connected());
    CHECK(nh.clientCount() == 1);
}

// ---------------- Socket abstraction (Winsock2 port) ----------------
// Exercises the wrapper API directly so the POSIX/Winsock2 branches share
// one behavioral contract: validity transitions, close() idempotence,
// ephemeral ports, and a UDP + TCP loopback round-trip.

static void testSocketAbstraction() {
    // Fresh sockets start invalid; close() on an invalid socket is safe.
    {
        UdpSocket u;
        TcpSocket t;
        TcpListener l;
        CHECK(!u.valid());
        CHECK(!t.valid());
        CHECK(!l.valid());
        u.close(); t.close(); l.close();  // must not crash
        CHECK(!u.valid());
    }
    // UDP loopback round-trip through the abstraction (fixed high ports).
    // Sandbox environments sometimes block UDP outright (EPERM on
    // sendto); probe first and skip like testDiscoveryLoopback does.
    {
        UdpSocket probe;
        if (!probe.open() || !probe.sendTo("127.0.0.1", 47999, "x", 1)) {
            std::cout << "SKIP testSocketAbstraction UDP part (UDP blocked)\\n";
        } else {
            UdpSocket s1, s2;
            CHECK(s1.open() && s2.open());
            CHECK(s1.valid() && s2.valid());
            CHECK(s1.bind(47991) && s2.bind(47992));
            const char* msg = "winsock-probe";
            CHECK(s1.sendTo("127.0.0.1", 47992, msg, 13));
            char buf[64] = {};
            std::string fromIp; uint16_t fromPort = 0;
            long n = s2.recvFrom(buf, sizeof(buf), fromIp, fromPort, 2000);
            CHECK(n == 13);
            CHECK(std::string(buf, 13) == msg);
            CHECK(fromIp == "127.0.0.1");
            CHECK(fromPort == 47991);
            s1.close(); s2.close();
            CHECK(!s1.valid() && !s2.valid());
            s1.close();  // double close safe
        }
    }
    // TCP loopback: listen(0) -> connect -> accept -> echo.
    {
        TcpListener listener;
        CHECK(listener.listen(0));
        uint16_t port = listener.boundPort();
        CHECK(port != 0);
        TcpSocket client;
        CHECK(client.connect("127.0.0.1", port, 3000));
        CHECK(client.valid());
        auto acc = listener.accept(3000);
        CHECK(acc.has_value());
        CHECK(acc->valid());
        const char* ping = "tcp-probe-123";
        CHECK(client.sendAll(ping, 13));
        char buf[64] = {};
        CHECK(acc->recvSome(buf, sizeof(buf), 2000) == 13);
        CHECK(std::string(buf, 13) == ping);
        CHECK(acc->sendAll(buf, 13));
        char back[64] = {};
        CHECK(client.recvSome(back, sizeof(back), 2000) == 13);
        CHECK(std::string(back, 13) == ping);
        // Move semantics transfer ownership; source goes invalid.
        TcpSocket moved = std::move(client);
        CHECK(moved.valid());
        CHECK(!client.valid());
    }
}

int main() {
    testProtocolRoundTrip();
    testSocketAbstraction();
    testMessageReaderPartial();
    testRadminClassify();
    testBeaconParse();
    testDiscoveryLoopback();
    testLobbyHandshake();
    testInputSnapshotCodec();
    testNetcodeLoopback();

    std::cout << "net checks: " << checks << ", failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}
