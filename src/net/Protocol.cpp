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

} // namespace net
} // namespace cultulhu
