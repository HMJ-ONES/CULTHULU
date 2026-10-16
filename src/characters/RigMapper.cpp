#include "characters/RigMapper.h"

#include "animation/ProceduralClips.h"

#include <cctype>
#include <sstream>
#include <unordered_map>

namespace cultulhu {

const std::vector<std::string>& RigMapper::engineBones() {
    return humanoidBones();
}

std::string RigMapper::normalize(const std::string& name) {
    std::string s;
    s.reserve(name.size());
    for (char ch : name) s.push_back(static_cast<char>(std::tolower(
                             static_cast<unsigned char>(ch))));
    const std::string prefix = "mixamorig:";
    if (s.rfind(prefix, 0) == 0) s = s.substr(prefix.size());
    // Rigify deform bones ("DEF-thigh.L") and Blender "DEF_" prefixes:
    // strip the deform marker so they match the base bone names.
    if (s.rfind("def", 0) == 0) s = s.substr(3);
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (std::isalnum(static_cast<unsigned char>(ch))) out.push_back(ch);
    }
    return out;
}

const std::vector<std::string>& RigMapper::aliasesFor(
    const std::string& engineBone) {
    // Normalized alias lists (compare against normalize(candidate)).
    // Covers Mixamo ("LeftForeArm"), Blender default ("Forearm.L"),
    // Rigify ("forearm.L", "DEF-forearm.L" via the def-strip), and plain
    // DCC names ("forearm_l", "l_forearm", "Clavicle_L").
    static const std::unordered_map<std::string, std::vector<std::string>>
        table = {
            {"hips",
             {"pelvis", "hip", "root", "hipbone", "crotch", "pelvisl",
              "pelvisr"}},
            {"spine",
             {"chest", "spine1", "spine2", "torso", "upperchest", "belly",
              "waist", "spine01", "spine02"}},
            {"head", {"neckhead", "neck", "headneck"}},
            {"upperarml",
             {"leftarm", "armleft", "shoulderl", "upperarml", "lupperarm",
              "collarl", "claviclel"}},
            {"upperarmr",
             {"rightarm", "armright", "shoulderr", "upperarmr", "rupperarm",
              "collarr", "clavicler"}},
            {"lowerarml",
             {"leftforearm", "forearmleft", "forearml", "elbowl",
              "lforearm"}},
            {"lowerarmr",
             {"rightforearm", "forearmright", "forearmr", "elbowr",
              "rforearm"}},
            {"upperlegl",
             {"leftupleg", "uplegleft", "thighl", "hiplegl", "legl",
              "lthigh", "lupperleg"}},
            {"upperlegr",
             {"rightupleg", "uplegright", "thighr", "hiplegr", "legr",
              "rthigh", "rupperleg"}},
            {"lowerlegl",
             {"leftleg", "legleft", "shinl", "calfl", "kneel", "lshin"}},
            {"lowerlegr",
             {"rightleg", "legright", "shinr", "calfr", "kneer", "rshin"}},
        };
    // NOTE: normalize() lowercases, strips "mixamorig:"/"DEF" prefixes and
    // drops non-alphanumerics, so "upper_arm.L" -> "upperarml",
    // "DEF-thigh.L" -> "thighl", "Clavicle_L" -> "claviclel" and
    // "mixamorig:LeftForeArm" -> "leftforearm". Aliases above are stored
    // in normalized form.
    static const std::vector<std::string> empty;
    auto it = table.find(engineBone);
    return it == table.end() ? empty : it->second;
}

RigMapping RigMapper::mapBones(const std::vector<std::string>& fbxBones) {
    RigMapping mapping;
    std::vector<bool> claimed(fbxBones.size(), false);

    for (const std::string& engine : engineBones()) {
        BoneMap bm;
        bm.engineBone = engine;
        const std::string want = normalize(engine);

        // Pass 1: exact case-insensitive match on the engine name.
        // Pass 2: alias match.
        for (int pass = 0; pass < 2 && !bm.mapped; ++pass) {
            for (size_t i = 0; i < fbxBones.size(); ++i) {
                if (claimed[i]) continue;
                const std::string norm = normalize(fbxBones[i]);
                bool hit = false;
                if (pass == 0) {
                    hit = (norm == want);
                } else {
                    for (const std::string& a : aliasesFor(want)) {
                        if (norm == a) { hit = true; break; }
                    }
                }
                if (hit) {
                    bm.fbxBone = fbxBones[i];
                    bm.confidence = (pass == 0) ? 1.0f : 0.9f;
                    bm.mapped = true;
                    claimed[i] = true;
                    break;
                }
            }
        }
        mapping.bones.push_back(bm);
    }
    return mapping;
}

RigMapping RigMapper::parseRigMap(const std::string& text,
                                  std::vector<std::string>& warnings) {
    RigMapping mapping;
    for (const std::string& engine : engineBones()) {
        BoneMap bm;
        bm.engineBone = engine;
        mapping.bones.push_back(bm);
    }
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    auto trim = [](std::string s) {
        const char* ws = " \t\r\n";
        s.erase(0, s.find_first_not_of(ws));
        if (!s.empty()) s.erase(s.find_last_not_of(ws) + 1);
        return s;
    };
    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            warnings.push_back("rig.map line " + std::to_string(lineNo) +
                               ": no '=' — skipped");
            continue;
        }
        const std::string eng = trim(line.substr(0, eq));
        const std::string fbx = trim(line.substr(eq + 1));
        bool known = false;
        for (auto& bm : mapping.bones) {
            if (bm.engineBone == eng) {
                bm.fbxBone = fbx;
                bm.confidence = 1.0f;
                bm.mapped = true;
                known = true;
                break;
            }
        }
        if (!known) {
            warnings.push_back("rig.map line " + std::to_string(lineNo) +
                               ": unknown engine bone '" + eng + "'");
        }
    }
    return mapping;
}

float RigMapping::overallConfidence() const {
    float sum = 0.0f;
    int n = 0;
    for (const auto& b : bones) {
        if (b.mapped) {
            sum += b.confidence;
            ++n;
        }
    }
    return n == 0 ? 0.0f : sum / n;
}

std::vector<std::string> RigMapping::unmapped() const {
    std::vector<std::string> out;
    for (const auto& b : bones) {
        if (!b.mapped) out.push_back(b.engineBone);
    }
    return out;
}

int RigMapping::mappedCount() const {
    int n = 0;
    for (const auto& b : bones) {
        if (b.mapped) ++n;
    }
    return n;
}

std::vector<std::string> RigMapping::diagnosticLines() const {
    std::vector<std::string> out;
    for (const auto& b : bones) {
        std::ostringstream ss;
        ss << b.engineBone;
        // Pad the engine-bone column for readability.
        for (size_t i = b.engineBone.size(); i < 11; ++i) ss << ' ';
        if (!b.mapped) {
            ss << "<- (unmapped)";
        } else {
            ss << "<- '" << b.fbxBone << "' (";
            ss << (b.confidence >= 1.0f ? "exact"
                   : b.confidence >= 0.9f ? "alias"
                                          : "low-confidence");
            ss << ", " << static_cast<int>(b.confidence * 100 + 0.5f)
               << "%)";
        }
        out.push_back(ss.str());
    }
    return out;
}

} // namespace cultulhu
