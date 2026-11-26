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

// Reads a JSON file into a value, for a module that owns the file's format.
Value read_json(const std::filesystem::path& file);

class Content {
public:
    std::map<Block, Definition> blocks;
    std::vector<std::filesystem::path> packs; // In order of their names.
    std::vector<std::string> layers;          // Front to back.

    // Loads every pack in the content root.
    void load(const std::filesystem::path& root);
    // The block's name; empty if no pack defines it.
    const std::string& name(const Block& id) const;
    // Each pack's copy of a file whose format a module owns, in pack order.
    std::vector<std::pair<std::filesystem::path, Value>> parts(const std::string& file) const;
};

}
