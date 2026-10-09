#include "discovery/DiscoveryCodex.h"

#include "save/SaveSystem.h"

#include <cctype>

namespace cultulhu {

namespace {

std::string slugify(const std::string& s) {
    std::string out;
    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c))
            out += static_cast<char>(std::tolower(c));
        else if (ch == ' ' || ch == '_' || ch == '-')
            out += '_';
    }
    return out.empty() ? "unknown" : out;
}

} // namespace

DiscoveryCodex::DiscoveryCodex(EventBus& bus) : bus_(bus) {}

std::string DiscoveryCodex::makeId(DiscoveryKind k, const std::string& key) {
    return std::string(discoveryKindName(k)) + ":" + slugify(key);
}

bool DiscoveryCodex::discover(DiscoveryKind kind, const std::string& key,
                              const std::string& name,
                              const std::string& flavor, Vec3 pos,
                              double gameTime, bool night) {
    const std::string id = makeId(kind, key);
    if (find(id) != nullptr) return false; // already logged

    Discovery d;
    d.id = id;
    d.kind = kind;
    d.name = name.empty() ? key : name;
    d.flavor = flavor;
    d.pos = pos;
    d.gameTime = gameTime;
    d.night = night;
    discoveries_.push_back(d);

    GameEvent e(EventType::DiscoveryMade);
    e.tag = id;
    e.amount = powerReward(night);
    e.faction = night ? 1 : 0;
    e.pos = pos;
    bus_.publish(e);
    return true;
}

bool DiscoveryCodex::rename(const std::string& id, const std::string& newName) {
    if (newName.empty()) return false;
    for (auto& d : discoveries_) {
        if (d.id == id) {
            d.name = newName;
            d.renamed = true;
            return true;
        }
    }
    return false;
}

const Discovery* DiscoveryCodex::find(const std::string& id) const {
    for (const auto& d : discoveries_)
        if (d.id == id) return &d;
    return nullptr;
}

size_t DiscoveryCodex::countKind(DiscoveryKind k) const {
    size_t n = 0;
    for (const auto& d : discoveries_)
        if (d.kind == k) ++n;
    return n;
}

void DiscoveryCodex::saveTo(GameState& s) const {
    s.discoveries.clear();
    for (const auto& d : discoveries_) {
        GameState::DiscoveryRec r;
        r.id = d.id;
        r.kind = static_cast<int>(d.kind);
        r.name = d.name;
        r.flavor = d.flavor;
        r.pos = d.pos;
        r.gameTime = d.gameTime;
        r.night = d.night;
        r.renamed = d.renamed;
        s.discoveries.push_back(r);
    }
}

void DiscoveryCodex::loadFrom(const GameState& s) {
    discoveries_.clear();
    for (const auto& r : s.discoveries) {
        Discovery d;
        d.id = r.id;
        d.kind = (r.kind >= 0 && r.kind < static_cast<int>(DiscoveryKind::Count))
                     ? static_cast<DiscoveryKind>(r.kind)
                     : DiscoveryKind::Landmark;
        d.name = r.name;
        d.flavor = r.flavor;
        d.pos = r.pos;
        d.gameTime = r.gameTime;
        d.night = r.night;
        d.renamed = r.renamed;
        discoveries_.push_back(d);
    }
}

} // namespace cultulhu
