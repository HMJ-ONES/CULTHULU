#include "driver/AntiCheat.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace {

std::string homeDir() {
    const char* h = std::getenv("HOME");
    if (h && *h) return h;
    return ".";
}

} // namespace

AntiCheat::AntiCheat() { load(); }

std::string AntiCheat::dataPath() const {
    return homeDir() + "/.cultulhu/anticheat.dat";
}

void AntiCheat::load() {
    std::ifstream f(dataPath());
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("warnings=", 0) == 0)
            warnings_ = std::stoi(line.substr(9));
        else if (line.rfind("banned=", 0) == 0)
            banned_ = (line.substr(7) == "1");
    }
}

void AntiCheat::save() const {
    std::string dir = homeDir() + "/.cultulhu";
    std::error_code error;
    std::filesystem::create_directories(dir,error);
    if(error) return;
    std::ofstream f(dataPath());
    if (!f) return;
    f << "warnings=" << warnings_ << "\n";
    f << "banned=" << (banned_ ? "1" : "0") << "\n";
}

bool AntiCheat::recordCommand(const std::string& cmd) {
    if (banned_) return true; // already banned: nothing to detect
    auto now = std::chrono::steady_clock::now();
    history_.push_back({cmd, now});
    if (history_.size() > 64) history_.erase(history_.begin());
    return !(checkBurst() || checkRegularity());
}

bool AntiCheat::checkBurst() const {
    if (history_.size() < kBurstCount) return false;
    auto newest = history_.back().t;
    auto oldest = history_[history_.size() - kBurstCount].t;
    return (newest - oldest) < kBurstWindow;
}

bool AntiCheat::checkRegularity() const {
    if (history_.size() < kRegularCount) return false;
    // Last N commands must be identical.
    const std::string& last = history_.back().cmd;
    for (size_t i = history_.size() - kRegularCount;
         i < history_.size(); ++i) {
        if (history_[i].cmd != last) return false;
    }
    // Intervals must be suspiciously regular (low stddev).
    std::vector<double> gaps;
    for (size_t i = history_.size() - kRegularCount + 1;
         i < history_.size(); ++i) {
        auto d = history_[i].t - history_[i - 1].t;
        gaps.push_back(
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(d)
                    .count()));
    }
    double mean = 0;
    for (double g : gaps) mean += g;
    mean /= gaps.size();
    if (mean < 50.0) return true; // faster than any human
    double var = 0;
    for (double g : gaps) var += (g - mean) * (g - mean);
    var /= gaps.size();
    return std::sqrt(var) <
           static_cast<double>(kRegularTolerance.count());
}

bool AntiCheat::issueWarning() {
    ++warnings_;
    if (warnings_ >= kMaxWarnings) {
        banned_ = true;
        wipeProgression();
    }
    save();
    return banned_;
}

void AntiCheat::wipeProgression() {
    // Ban = progression lost. The in-memory achievements are reset by the
    // driver; local save files are left alone (they're the player's own
    // files), but the ban flag persists — multiplayer stays blocked.
    // (Future: cloud progression would be wiped server-side.)
}

std::string AntiCheat::status() const {
    std::ostringstream s;
    s << "anti-cheat: ";
    if (banned_) {
        s << "BANNED — bot matches only. Progression wiped.";
    } else if (warnings_ > 0) {
        s << warnings_ << "/3 warnings. Next detection = "
          << (warnings_ + 1 >= kMaxWarnings ? "BAN." : "warning.");
    } else {
        s << "clean. 3 detections = ban.";
    }
    return s.str();
}
