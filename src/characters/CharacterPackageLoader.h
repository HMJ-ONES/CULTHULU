#pragma once

// CULT-ULHU character package auto-discovery (wave 7).
//
// CharacterPackageLoader scans assets/characters/ at startup: every
// subfolder is loaded as a package, validated, and its CharacterDef is
// registered in the CharacterRegistry. Adding a character needs no code
// changes — just drop the folder in. Packages also hot-load at runtime
// (driver `addchar`).

#include "characters/CharacterPackage.h"
#include "characters/CharacterRegistry.h"

#include <string>
#include <vector>

namespace cultulhu {

class CharacterPackageLoader {
public:
    explicit CharacterPackageLoader(CharacterRegistry& registry);

    // Scan root for subfolders and load each as a package. Returns the
    // number of packages whose character.def parsed (invalid ones are
    // skipped with the reason logged into loadErrors()).
    int scanAndLoad(const std::string& root);

    // Load one package folder at runtime. Returns true when the def
    // parsed and registered (replaces any package with the same folder).
    bool hotLoad(const std::string& folderPath);

    const std::vector<CharacterPackage>& packages() const { return packages_; }
    const CharacterPackage* find(const std::string& folderName) const;
    const std::vector<std::string>& loadErrors() const { return errors_; }

private:
    CharacterPackage loadOne(const std::string& folderPath);
    static std::string readFile(const std::string& path, bool& ok);
    static std::vector<std::string> readLines(const std::string& path,
                                              bool& ok);
    static bool fileExists(const std::string& path);
    static std::vector<std::string> listCanim(const std::string& dir);

    CharacterRegistry& registry_;
    std::vector<CharacterPackage> packages_;
    std::vector<std::string> errors_;
};

} // namespace cultulhu
