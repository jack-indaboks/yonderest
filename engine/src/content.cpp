#include "yonderest/content.hpp"
#include "json.hpp"

#include <algorithm>

namespace yonder {

void Content::load(const std::filesystem::path& root) {
    for (const auto& entry : std::filesystem::directory_iterator(root)) packs.push_back(entry.path());
    std::ranges::sort(packs);
    for (const auto& [path, list] : parts("layers.json"))
        for (const auto& name : *list.array()) layers.push_back(Field(name, path.string()).text());
    for (const auto& [path, list] : parts("blocks.json")) {
        Field entries(list, path.string());
        for (std::size_t n = 0; n < entries.size(); ++n) {
            auto entry = entries[n];
            blocks[entry["id"].text()] = {entry["name"].text(), rest(entry.value(), {"id", "name"})};
        }
    }
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

}
