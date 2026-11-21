#include "json.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace yonder {
namespace {

[[noreturn]] void invalid(const std::filesystem::path& path, const std::string& what) {
    throw std::runtime_error("Invalid content in " + path.string() + ": " + what);
}

int coordinate(const Field& field) { return int(field.integer(0, 255)); }

void load_recipes(Content& content, const std::filesystem::path& pack) {
    auto path = pack / "recipes.json";
    if (!std::filesystem::exists(path)) return;
    JsonFile file(path);
    auto list = file.root();
    for (Json::ArrayIndex n = 0; n < list.size(); ++n) {
        auto entry = list[n];
        Recipe r;
        r.id = entry["id"].text();
        auto output = entry["output"];
        r.output = output["block"].text();
        r.amount = int(output["count"].integer_or(1, 1, 1000000));
        r.output_x = coordinate(output["x"]);
        r.output_y = coordinate(output["y"]);
        // Rows run down and cells run right. A block ID is an ingredient; null must stay empty.
        auto rows = entry["shape"];
        for (Json::ArrayIndex y = 0; y < rows.size(); ++y)
            for (Json::ArrayIndex x = 0; x < rows[y].size(); ++x) {
                auto cell = rows[y][x];
                r.shape.push_back({int(x), int(y), cell.present() ? cell.text() : Block{}});
                if (cell.present() && r.shape.back().id.empty()) cell.fail("must name a block, or be null for an empty cell");
                r.width = std::max(r.width, int(x) + 1);
                r.height = std::max(r.height, int(y) + 1);
            }
        auto stations = entry["stations"];
        for (Json::ArrayIndex i = 0; i < stations.size(); ++i) r.stations.push_back(stations[i].text());
        bool output_in_ingredients = std::ranges::any_of(r.shape, [&](const Ingredient& i) {
            return i.x == r.output_x && i.y == r.output_y && !i.id.empty();
        });
        if (!output_in_ingredients) invalid(path, "output must replace an ingredient: " + r.id);
        r.order = int(content.recipes.size());
        content.recipes.push_back(r);
    }
}

void load_poses(Content& content, const std::filesystem::path& pack) {
    auto path = pack / "character.json";
    if (!std::filesystem::exists(path)) return;
    JsonFile file(path);
    auto poses = file.root()["poses"];
    for (const auto& name : poses.keys()) {
        auto entry = poses[name.c_str()];
        auto pixel = [](const Field& f) { return int(f.integer(0, 15)); };
        Pose p;
        p.image = entry["image"].text();
        p.bare = entry["bare"].text_or("");
        p.anchor = pixel(entry["anchor"]);
        p.hand_x = pixel(entry["hand"]["x"]);
        p.hand_y = pixel(entry["hand"]["y"]);
        // A carried block rests beside the hand unless the pose places it.
        auto block = entry["block"];
        p.block_x = block.present() ? pixel(block["x"]) : p.hand_x + 1;
        p.block_y = block.present() ? pixel(block["y"]) : p.hand_y - 3;
        if (p.block_x > 15 || p.block_y < 0) entry.fail("needs a block position: the default beside the hand is off the canvas");
        content.poses[name] = p;
    }
}

}

void Content::load(const std::filesystem::path& pack) {
    if (std::filesystem::exists(pack / "module.json")) {
        JsonFile file(pack / "module.json");
        auto root = file.root();
        auto id = root["id"].text();
        Module module{int(root["version"].integer(1, 1000000)), {}};
        auto requires_modules = root["requires"];
        for (const auto& dependency : requires_modules.keys())
            module.requires_modules[dependency] = int(requires_modules[dependency.c_str()].integer(1, 1000000));
        if (!modules.emplace(id, module).second) invalid(pack, "duplicate module " + id);
    }
    JsonFile file(pack / "blocks.json");
    auto list = file.root();
    for (Json::ArrayIndex n = 0; n < list.size(); ++n) {
        auto entry = list[n];
        Block id = entry["id"].text();
        Material m;
        auto color = entry["color"];
        if (color.size() != 3) color.fail("must be three numbers: red, green, blue");
        for (Json::ArrayIndex c = 0; c < 3; ++c) m.color[c] = int(color[c].integer(0, 255));
        m.solid = entry["solid"].flag_or(true);
        m.name = entry["name"].text();
        m.station = entry["station"].text_or("");
        m.tool = entry["tool"].text_or("");
        m.requires_tool = entry["requires_tool"].text_or("");
        auto name = id.substr(id.find(':') + 1) + ".png";
        if (std::filesystem::exists(pack / "blocks" / name)) m.texture = pack / "blocks" / name;
        if (std::filesystem::exists(pack / "held" / name)) m.held = pack / "held" / name;
        if (!blocks.emplace(id, m).second) throw std::runtime_error("Duplicate block ID: " + id);
    }
    if (std::filesystem::is_directory(pack / "images"))
        for (const auto& entry : std::filesystem::directory_iterator(pack / "images"))
            if (entry.path().extension() == ".png") images[entry.path().stem().string()] = entry.path();
    load_poses(*this, pack);
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
    JsonFile file(pack / "terrain.json");
    return read_terrain(file.root());
}

Manifest load_manifest(const std::filesystem::path& path) {
    JsonFile file(path);
    auto root = file.root();
    Manifest m{};
    m.version = int(root["version"].integer(1, 1000000));
    auto packs = root["packs"];
    for (Json::ArrayIndex n = 0; n < packs.size(); ++n)
        m.packs.emplace_back(packs[n]["id"].text(), int(packs[n]["version"].integer(1, 1000000)));
    m.terrain = root["terrain"].text();
    if (m.packs.empty()) invalid(path, "no packs");
    if (std::ranges::none_of(m.packs, [&](const auto& p) { return p.first == m.terrain; })) invalid(path, "terrain pack must be enabled");
    return m;
}

}
