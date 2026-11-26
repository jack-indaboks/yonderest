#pragma once

// Packs: modules without code, carrying blocks, layers and parts that belong to other modules.

#include "yonderest/space.hpp"
#include "yonderest/value.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace yonder {

// A block's definition: its name, beside its identity, and the properties modules give it.
struct Definition {
    std::string name;
    Properties properties;
};

struct Module {
    std::filesystem::path folder;
    int version;
    std::map<std::string, int> requires_modules;
};

// The flagship manifest: which packs make up the game, and settings for its modules.
struct Manifest {
    int version;
    std::vector<std::pair<std::string, int>> packs;
    Properties properties;
};
Manifest load_manifest(const std::filesystem::path& file);

// Reads a JSON file into a value, for a module that owns the file's format.
Value read_json(const std::filesystem::path& file);

class Content {
public:
    std::map<Block, Definition> blocks;
    std::map<std::string, Module> modules;
    std::vector<std::filesystem::path> packs; // In load order: later packs override earlier ones.
    std::vector<std::string> layers;          // Front to back.
    Properties manifest;                      // The manifest's settings for modules.

    void load(const std::filesystem::path& pack);
    // Loads the manifest's packs from the content root, checking their versions.
    void load_flagship(const std::filesystem::path& root, const std::filesystem::path& manifest);
    void validate() const;
    // Missing content keeps its identity; each module decides what missing means for it.
    bool defined(const Block& id) const { return blocks.contains(id); }
    // The block's name; empty if it is not defined.
    const std::string& name(const Block& id) const;
    // Each pack's copy of a file whose format a module owns, in pack order.
    std::vector<std::pair<std::filesystem::path, Value>> parts(const std::string& file) const;
};

}
