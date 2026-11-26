#include "yonderest/content.hpp"
#include "json.hpp"

#include <algorithm>
#include <stdexcept>

namespace yonder {
namespace {

[[noreturn]] void invalid(const std::filesystem::path& path, const std::string& what) {
    throw std::runtime_error("Invalid content in " + path.string() + ": " + what);
}

}

Value read_json(const std::filesystem::path& file) {
    return JsonFile(file).root().value();
}

void Content::load(const std::filesystem::path& pack) {
    packs.push_back(pack);
    if (std::filesystem::exists(pack / "module.json")) {
        JsonFile file(pack / "module.json");
        auto root = file.root();
        auto id = root["id"].text();
        Module module{pack, int(root["version"].integer(1, 1000000)), {}};
        auto requires_modules = root["requires"];
        for (const auto& dependency : requires_modules.keys())
            module.requires_modules[dependency] = int(requires_modules[dependency.c_str()].integer(1, 1000000));
        if (!modules.emplace(id, module).second) invalid(pack, "duplicate module " + id);
    }
    if (std::filesystem::exists(pack / "layers.json")) {
        JsonFile file(pack / "layers.json");
        auto list = file.root();
        for (std::size_t n = 0; n < list.size(); ++n) {
            auto name = list[n].text();
            if (std::ranges::find(layers, name) != layers.end()) list[n].fail("is a layer already declared");
            layers.push_back(name);
        }
    }
    if (std::filesystem::exists(pack / "blocks.json")) {
        JsonFile file(pack / "blocks.json");
        auto list = file.root();
        for (std::size_t n = 0; n < list.size(); ++n) {
            auto entry = list[n];
            Block id = entry["id"].text();
            Definition definition{entry["name"].text(), file.rest(entry, {"id", "name"})};
            if (!blocks.emplace(id, std::move(definition)).second) throw std::runtime_error("Duplicate block ID: " + id);
        }
    }
}

void Content::load_flagship(const std::filesystem::path& root, const std::filesystem::path& path) {
    auto manifest = load_manifest(path);
    for (const auto& [id, version] : manifest.packs) {
        load(root / id);
        auto found = modules.find(id);
        if (found == modules.end() || found->second.version != version) throw std::runtime_error("Manifest version mismatch for " + id);
    }
    this->manifest = std::move(manifest.properties);
}

void Content::validate() const {
    for (const auto& [id, module] : modules)
        for (const auto& [dependency, version] : module.requires_modules) {
            auto found = modules.find(dependency);
            if (found == modules.end() || found->second.version < version)
                throw std::runtime_error(id + " requires " + dependency + " version " + std::to_string(version));
        }
    if (layers.empty()) throw std::runtime_error("No pack declares the layers of a cell");
}

const std::string& Content::name(const Block& id) const {
    static const std::string none;
    auto found = blocks.find(id);
    return found == blocks.end() ? none : found->second.name;
}

std::vector<std::pair<std::filesystem::path, Value>> Content::parts(const std::string& file) const {
    std::vector<std::pair<std::filesystem::path, Value>> result;
    for (const auto& pack : packs)
        if (std::filesystem::exists(pack / file)) result.emplace_back(pack / file, read_json(pack / file));
    return result;
}

Manifest load_manifest(const std::filesystem::path& path) {
    JsonFile file(path);
    auto root = file.root();
    Manifest m{};
    m.version = int(root["version"].integer(1, 1000000));
    auto packs = root["packs"];
    for (std::size_t n = 0; n < packs.size(); ++n)
        m.packs.emplace_back(packs[n]["id"].text(), int(packs[n]["version"].integer(1, 1000000)));
    if (m.packs.empty()) invalid(path, "no packs");
    m.properties = file.rest(root, {"version", "packs"});
    return m;
}

}
