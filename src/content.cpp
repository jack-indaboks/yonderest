#include "world.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace yonder {
namespace {

std::vector<std::string> lines(const std::filesystem::path& path, bool optional = false) {
    std::ifstream file(path);
    if (!file && !optional) throw std::runtime_error("Cannot read " + path.string());
    std::vector<std::string> result;
    std::string line;
    while (std::getline(file, line)) {
        auto first = line.find_first_not_of(" \t\r");
        if (first != std::string::npos && line[first] != '#') result.push_back(line);
    }
    return result;
}

[[noreturn]] void invalid(const std::filesystem::path& path, const std::string& line) {
    throw std::runtime_error("Invalid content in " + path.string() + ": " + line);
}

void load_recipes(Content& content, const std::filesystem::path& pack) {
    Recipe r;
    for (const auto& line : lines(pack / "recipes.txt", true)) {
        std::istringstream in(line);
        std::string op;
        in >> op;
        if (op == "recipe" && r.id.empty()) {
            if (!(in >> r.id >> r.output >> r.amount >> r.output_x >> r.output_y) || r.amount <= 0) invalid(pack, line);
        } else if ((op == "at" || op == "empty") && !r.id.empty()) {
            Ingredient i;
            if (!(in >> i.x >> i.y) || i.x < 0 || i.y < 0) invalid(pack, line);
            if (op == "at" && !(in >> i.id)) invalid(pack, line);
            r.width = std::max(r.width, i.x + 1);
            r.height = std::max(r.height, i.y + 1);
            r.shape.push_back(i);
        } else if (op == "requires" && !r.id.empty()) {
            std::string station;
            if (!(in >> station)) invalid(pack, line);
            r.stations.push_back(station);
        } else if (op == "end" && !r.id.empty()) {
            std::set<std::pair<int, int>> cells;
            bool output_in_ingredients = false;
            for (const auto& i : r.shape) {
                if (!cells.emplace(i.x, i.y).second) invalid(pack, "overlapping recipe cells: " + r.id);
                output_in_ingredients |= i.x == r.output_x && i.y == r.output_y && !i.id.empty();
            }
            if (!output_in_ingredients) invalid(pack, "output must replace an ingredient: " + r.id);
            r.order = int(content.recipes.size());
            content.recipes.push_back(r);
            r = {};
        } else invalid(pack, line);
    }
    if (!r.id.empty()) invalid(pack, "unterminated recipe: " + r.id);
}

}

void Content::load(const std::filesystem::path& pack) {
    std::string module_id;
    Module module{1, {}};
    for (const auto& line : lines(pack / "module.txt", true)) {
        std::istringstream in(line);
        std::string op, id;
        int version;
        if (!(in >> op >> id >> version) || version < 1) invalid(pack, line);
        if (op == "module" && module_id.empty()) { module_id = id; module.version = version; }
        else if (op == "requires" && !module_id.empty()) module.requires_modules[id] = version;
        else invalid(pack, line);
    }
    if (!module_id.empty() && !modules.emplace(module_id, module).second) invalid(pack, "duplicate module " + module_id);
    for (const auto& line : lines(pack / "blocks.txt")) {
        std::istringstream in(line);
        Block id;
        Material m;
        if (!(in >> id >> m.color[0] >> m.color[1] >> m.color[2] >> m.solid >> std::quoted(m.name))) invalid(pack, line);
        in >> m.station >> m.tool >> m.requires_tool;
        if (m.station == "-") m.station.clear();
        if (m.tool == "-") m.tool.clear();
        if (m.requires_tool == "-") m.requires_tool.clear();
        auto name = id.substr(id.find(':') + 1) + ".png";
        if (std::filesystem::exists(pack / "blocks" / name)) m.texture = pack / "blocks" / name;
        if (std::filesystem::exists(pack / "held" / name)) m.held = pack / "held" / name;
        if (!blocks.emplace(id, m).second) throw std::runtime_error("Duplicate block ID: " + id);
    }
    if (std::filesystem::is_directory(pack / "images"))
        for (const auto& entry : std::filesystem::directory_iterator(pack / "images"))
            if (entry.path().extension() == ".png") images[entry.path().stem().string()] = entry.path();
    for (const auto& line : lines(pack / "character.txt", true)) {
        std::istringstream in(line);
        std::string op, name;
        in >> op >> name;
        if (op == "bare") {
            auto found = poses.find(name);
            if (found == poses.end() || !(in >> found->second.bare)) invalid(pack, line);
            continue;
        }
        Pose p;
        if (op != "pose" || !(in >> p.image >> p.anchor >> p.hand_x >> p.hand_y)) invalid(pack, line);
        if (!(in >> p.block_x >> p.block_y)) { p.block_x = p.hand_x + 1; p.block_y = p.hand_y - 3; }
        for (int v : {p.anchor, p.hand_x, p.hand_y, p.block_x, p.block_y})
            if (v < 0 || v > 15) invalid(pack, line);
        poses[name] = p;
    }
    load_recipes(*this, pack);
    // Larger, more specific patterns win. Pack load order grants no priority.
    std::ranges::sort(recipes, [](const Recipe& a, const Recipe& b) {
        if (a.shape.size() != b.shape.size()) return a.shape.size() > b.shape.size();
        return a.id < b.id;
    });
}

void Content::validate() const {
    for (const auto& [id, module] : modules)
        for (const auto& [dependency, version] : module.requires_modules) {
            auto found = modules.find(dependency);
            if (found == modules.end() || found->second.version < version)
                throw std::runtime_error(id + " requires " + dependency + " version " + std::to_string(version));
        }
    std::set<std::string> ids;
    for (const auto& r : recipes) {
        if (!ids.insert(r.id).second) throw std::runtime_error("Duplicate recipe: " + r.id);
        if (!blocks.contains(r.output)) throw std::runtime_error("Undefined recipe output: " + r.output);
        for (const auto& i : r.shape)
            if (!i.id.empty() && !blocks.contains(i.id)) throw std::runtime_error("Undefined recipe ingredient: " + i.id);
    }
}

const Material& Content::material(const Block& id) const {
    static const Material air{{0, 0, 0}, false, "Air"};
    static const Material missing{{151, 104, 88}, true, "Unknown"};
    if (id.empty()) return air;
    auto found = blocks.find(id);
    return found == blocks.end() ? missing : found->second;
}

Terrain load_terrain(const std::filesystem::path& pack) {
    auto text = lines(pack / "terrain.txt");
    std::string joined;
    for (const auto& line : text) joined += line + '\n';
    std::istringstream in(joined);
    Terrain t;
    if (!(in >> t.version >> t.grass >> t.soil >> t.stone >> t.sand >> t.timber >> t.leaves)) invalid(pack, "terrain roles");
    int rank = profile_rank(t.version);
    if (rank == 0) invalid(pack, "unsupported terrain profile " + t.version);
    if (rank >= 2 && !(in >> t.clay >> t.ore)) invalid(pack, "expedition terrain roles");
    return t;
}

Manifest load_manifest(const std::filesystem::path& file) {
    Manifest m{};
    for (const auto& line : lines(file)) {
        std::istringstream in(line);
        std::string op, id;
        int version;
        in >> op;
        if (op == "yonderest" && m.version == 0) {
            if (!(in >> m.version) || m.version != 2) invalid(file, line);
        } else if (op == "pack") {
            if (!(in >> id >> version) || version < 1) invalid(file, line);
            m.packs.emplace_back(id, version);
        } else if (op == "terrain") {
            if (!(in >> m.terrain)) invalid(file, line);
        } else invalid(file, line);
    }
    if (m.version != 2 || m.packs.empty() || m.terrain.empty()) invalid(file, "incomplete manifest");
    if (std::ranges::none_of(m.packs, [&](const auto& p) { return p.first == m.terrain; })) invalid(file, "terrain pack must be enabled");
    return m;
}

}
