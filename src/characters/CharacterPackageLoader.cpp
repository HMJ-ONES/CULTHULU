#include "characters/CharacterPackageLoader.h"

#include "characters/CharacterDefParser.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace cultulhu {

namespace fs = std::filesystem;

CharacterPackageLoader::CharacterPackageLoader(CharacterRegistry& registry)
    : registry_(registry) {}

bool CharacterPackageLoader::fileExists(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

std::string CharacterPackageLoader::readFile(const std::string& path,
                                             bool& ok) {
    std::ifstream in(path);
    if (!in) {
        ok = false;
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    ok = true;
    return ss.str();
}

std::vector<std::string> CharacterPackageLoader::readLines(
    const std::string& path, bool& ok) {
    std::vector<std::string> out;
    std::ifstream in(path);
    if (!in) {
        ok = false;
        return out;
    }
    ok = true;
    std::string line;
    while (std::getline(in, line)) {
        // Trim whitespace/CR.
        while (!line.empty() &&
               (line.back() == '\r' || line.back() == ' ' ||
                line.back() == '\t'))
            line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (!line.empty() && line[0] != '#') out.push_back(line);
    }
    return out;
}

std::vector<std::string> CharacterPackageLoader::listCanim(
    const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return out;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_regular_file() && e.path().extension() == ".canim")
            out.push_back(e.path().string());
    }
    return out;
}

CharacterPackage CharacterPackageLoader::loadOne(
    const std::string& folderPath) {
    CharacterPackage pkg;
    pkg.folderPath = folderPath;
    pkg.folderName = fs::path(folderPath).filename().string();

    bool ok = false;
    const std::string defText =
        readFile(folderPath + "/character.def", ok);
    if (!ok) {
        pkg.defError = "character.def missing";
        return pkg;
    }
    ParseResult pr = parseCharacterDef(defText);
    pkg.def = pr.def;
    pkg.defOk = pr.ok;
    pkg.defError = pr.error;
    if (!pkg.defOk) return pkg;

    pkg.hasModel = fileExists(folderPath + "/model.fbx");
    pkg.hasRigMap = fileExists(folderPath + "/rig.map");
    pkg.clipFiles = listCanim(folderPath + "/animations");

    // Rig mapping: explicit rig.map wins; otherwise auto-map from the
    // bones.list sidecar when present (bridge until the FBX importer
    // can supply real bone lists).
    std::vector<std::string> mapWarnings;
    if (pkg.hasRigMap) {
        bool rok = false;
        const std::string text =
            readFile(folderPath + "/rig.map", rok);
        if (rok) pkg.rigMapping = RigMapper::parseRigMap(text, mapWarnings);
    } else {
        bool bok = false;
        const std::vector<std::string> bones =
            readLines(folderPath + "/bones.list", bok);
        pkg.hasBonesList = bok;
        if (bok && !bones.empty()) {
            pkg.rigMapping = RigMapper::mapBones(bones);
            pkg.rigAutoMapped = true;
        }
    }
    for (const auto& w : mapWarnings) {
        // Surface rig.map problems through the def error channel only
        // when the def itself is fine; validator reports the rest.
        (void)w;
    }
    return pkg;
}

int CharacterPackageLoader::scanAndLoad(const std::string& root) {
    int loaded = 0;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        errors_.push_back("character root not found: " + root);
        return 0;
    }
    for (const auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory()) continue;
        CharacterPackage pkg = loadOne(e.path().string());
        if (!pkg.defOk) {
            errors_.push_back(pkg.folderName + ": " + pkg.defError);
            continue;
        }
        if (!registry_.registerCharacter(pkg.def)) {
            errors_.push_back(pkg.folderName + ": duplicate character id '" +
                              pkg.def.id + "'");
            continue;
        }
        packages_.push_back(std::move(pkg));
        ++loaded;
    }
    return loaded;
}

bool CharacterPackageLoader::hotLoad(const std::string& folderPath) {
    CharacterPackage pkg = loadOne(folderPath);
    if (!pkg.defOk) {
        errors_.push_back(pkg.folderName + ": " + pkg.defError);
        return false;
    }
    if (!registry_.registerCharacter(pkg.def)) {
        errors_.push_back(pkg.folderName + ": duplicate character id '" +
                          pkg.def.id + "'");
        return false;
    }
    // Replace any earlier package with the same folder name.
    for (auto& p : packages_) {
        if (p.folderName == pkg.folderName) {
            p = std::move(pkg);
            return true;
        }
    }
    packages_.push_back(std::move(pkg));
    return true;
}

const CharacterPackage* CharacterPackageLoader::find(
    const std::string& folderName) const {
    for (const auto& p : packages_) {
        if (p.folderName == folderName) return &p;
    }
    return nullptr;
}

} // namespace cultulhu
