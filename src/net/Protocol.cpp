#include "net/Protocol.h"

namespace cultulhu {
namespace net {
namespace {

std::string sanitize(const std::string& v) {
    std::string out;
    out.reserve(v.size());
    for (char c : v) {
        if (c == ';' || c == '=' || c == '\n' || c == '\r') continue;
        out.push_back(c);
    }
    return out;
}

} // namespace

std::string encodeFields(const std::map<std::string, std::string>& fields) {
    std::string out;
    for (const auto& [k, v] : fields) {
        out += sanitize(k);
        out += '=';
        out += sanitize(v);
        out += ';';
    }
    return out;
}

std::map<std::string, std::string> decodeFields(const std::string& payload) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while (i < payload.size()) {
        size_t semi = payload.find(';', i);
        std::string pair = payload.substr(i, semi == std::string::npos
                                                 ? semi : semi - i);
        size_t eq = pair.find('=');
        if (eq != std::string::npos)
            out[pair.substr(0, eq)] = pair.substr(eq + 1);
        if (semi == std::string::npos) break;
        i = semi + 1;
    }
    return out;
}

std::vector<uint8_t> encode(const Message& m) {
    std::string payload = encodeFields(m.fields);
    std::vector<uint8_t> out;
    out.reserve(5 + payload.size());
    out.push_back(static_cast<uint8_t>(m.type));
    uint32_t len = static_cast<uint32_t>(payload.size());
    out.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(len & 0xFF));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::optional<Message> decodeFrame(const uint8_t* data, size_t len) {
    if (len < 5) return std::nullopt;
    uint32_t payLen = (static_cast<uint32_t>(data[1]) << 24) |
                      (static_cast<uint32_t>(data[2]) << 16) |
                      (static_cast<uint32_t>(data[3]) << 8) |
                      static_cast<uint32_t>(data[4]);
    if (payLen > 1024 * 1024) return std::nullopt;  // sanity cap
    if (len < 5 + payLen) return std::nullopt;
    Message m;
    m.type = static_cast<MsgType>(data[0]);
    m.fields = decodeFields(
        std::string(reinterpret_cast<const char*>(data + 5), payLen));
    return m;
}

void MessageReader::feed(const uint8_t* data, size_t len) {
    buf_.insert(buf_.end(), data, data + len);
}

std::vector<Message> MessageReader::drain() {
    std::vector<Message> out;
    for (;;) {
        if (buf_.size() < 5) break;
        uint32_t payLen = (static_cast<uint32_t>(buf_[1]) << 24) |
                          (static_cast<uint32_t>(buf_[2]) << 16) |
                          (static_cast<uint32_t>(buf_[3]) << 8) |
                          static_cast<uint32_t>(buf_[4]);
        if (payLen > 1024 * 1024) { buf_.clear(); break; }  // corrupt: reset
        if (buf_.size() < 5 + payLen) break;
        if (auto m = decodeFrame(buf_.data(), 5 + payLen)) out.push_back(*m);
        buf_.erase(buf_.begin(), buf_.begin() + 5 + payLen);
    }
    return out;
}

std::string fieldStr(const Message& m, const std::string& key,
                     const std::string& dflt) {
    auto it = m.fields.find(key);
    return it == m.fields.end() ? dflt : it->second;
}

int fieldInt(const Message& m, const std::string& key, int dflt) {
    auto it = m.fields.find(key);
    if (it == m.fields.end()) return dflt;
    try { return std::stoi(it->second); } catch (...) { return dflt; }
}

float fieldFloat(const Message& m, const std::string& key, float dflt) {
    auto it = m.fields.find(key);
    if (it == m.fields.end()) return dflt;
    try { return std::stof(it->second); } catch (...) { return dflt; }
}

// ---------------- PlayerKda ----------------

namespace {

std::string cleanKdaName(const std::string& n) {
    // ',' would break the eK field split; ';' and '=' are stripped by
    // encodeFields anyway but removing them here keeps the contract
    // explicit and shared with decode.
    std::string out;
    for (char c : n) {
        if (c == ',' || c == ';' || c == '=') continue;
        out.push_back(c);
    }
    return out;
}

} // namespace

Message encodePlayerKda(const std::vector<KdaEntry>& entries) {
    Message m{MsgType::PlayerKda, {}};
    m.fields["n"] = std::to_string(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        m.fields["e" + std::to_string(i)] =
            std::to_string(e.playerIdx) + "," +
            std::to_string(e.row.kills) + "," +
            std::to_string(e.row.deaths) + "," +
            std::to_string(e.row.assists) + "," +
            cleanKdaName(e.row.name);
    }
    return m;
}

bool decodePlayerKda(const Message& m, std::vector<KdaEntry>& out) {
    if (m.type != MsgType::PlayerKda) return false;
    int n = fieldInt(m, "n", 0);
    // Wave 9d: 'n' is remote-controlled — cap it (see decodeSnapshot).
    if (n < 0 || n > 4096) return false;
    out.clear();
    for (int i = 0; i < n; ++i) {
        const std::string s = fieldStr(m, "e" + std::to_string(i));
        // "playerIdx,kills,deaths,assists,name" — exactly 5 parts; name
        // commas were stripped at encode, so a plain split is safe.
        std::vector<std::string> parts;
        size_t j = 0;
        while (j <= s.size()) {
            size_t c = s.find(',', j);
            parts.push_back(s.substr(j, c == std::string::npos ? c : c - j));
            if (c == std::string::npos) break;
            j = c + 1;
        }
        if (parts.size() != 5) continue;
        KdaEntry e;
        try {
            e.playerIdx = static_cast<uint32_t>(std::stoul(parts[0]));
            e.row.kills = std::stoi(parts[1]);
            e.row.deaths = std::stoi(parts[2]);
            e.row.assists = std::stoi(parts[3]);
            e.row.name = parts[4];
        } catch (...) {
            continue;
        }
        out.push_back(std::move(e));
    }
    return true;
}

Message encodeModeState(const ModeState& s) {
    Message m;
    m.type = MsgType::ModeState;
    m.fields["mode"] = s.mode;
    m.fields["s0"] = std::to_string(s.score0);
    m.fields["s1"] = std::to_string(s.score1);
    m.fields["np"] = std::to_string(s.pointOwners.size());
    for (size_t i = 0; i < s.pointOwners.size(); ++i) {
        m.fields["p" + std::to_string(i)] =
            std::to_string(s.pointOwners[i]) + "," +
            std::to_string(i < s.pointProg0.size() ? s.pointProg0[i] : 0.0f) + "," +
            std::to_string(i < s.pointProg1.size() ? s.pointProg1[i] : 0.0f);
    }
    m.fields["b0"] = std::to_string(s.baseHp0);
    m.fields["b1"] = std::to_string(s.baseHp1);
    m.fields["nt"] = std::to_string(s.towerHps.size());
    for (size_t i = 0; i < s.towerHps.size(); ++i)
        m.fields["t" + std::to_string(i)] = std::to_string(s.towerHps[i]);
    m.fields["nm"] = std::to_string(s.minionCount);
    return m;
}

bool decodeModeState(const Message& m, ModeState& out) {
    if (m.type != MsgType::ModeState) return false;
    out.mode = fieldStr(m, "mode", "none");
    out.score0 = fieldFloat(m, "s0", 0.0f);
    out.score1 = fieldFloat(m, "s1", 0.0f);
    out.pointOwners.clear();
    out.pointProg0.clear();
    out.pointProg1.clear();
    int np = fieldInt(m, "np", 0);
    for (int i = 0; i < np; ++i) {
        const std::string s = fieldStr(m, "p" + std::to_string(i));
        int owner = -1;
        float g0 = 0.0f, g1 = 0.0f;
        try {
            size_t c1 = s.find(',');
            size_t c2 = s.find(',', c1 + 1);
            owner = std::stoi(s.substr(0, c1));
            g0 = std::stof(s.substr(c1 + 1, c2 - c1 - 1));
            g1 = std::stof(s.substr(c2 + 1));
        } catch (...) {
            continue;
        }
        out.pointOwners.push_back(owner);
        out.pointProg0.push_back(g0);
        out.pointProg1.push_back(g1);
    }
    out.baseHp0 = fieldFloat(m, "b0", 0.0f);
    out.baseHp1 = fieldFloat(m, "b1", 0.0f);
    out.towerHps.clear();
    int nt = fieldInt(m, "nt", 0);
    for (int i = 0; i < nt; ++i)
        out.towerHps.push_back(fieldFloat(m, "t" + std::to_string(i), 0.0f));
    out.minionCount = fieldInt(m, "nm", 0);
    return true;
}

} // namespace net
} // namespace cultulhu
