// Crafting is construction: blocks standing in a recipe's arrangement, natural or placed,
// transform into what it makes. It layers over interaction: it says how many blocks a placement
// takes at a workbench and how many an unfinished ingredient gives back, cycles a workbench's
// batch, and reacts when a cell changes. Recipes are its own format, read from packs.

#include "yonderest/host.hpp"
#include "yonderest/inventory.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace yonder::crafting {
namespace {

struct Ingredient {
    int x, y;
    Block id; // Empty means this cell must be empty, not an ingredient.
};

struct Recipe {
    std::string id;
    int order = 0; // Definition order across packs, for teaching; matching uses size.
    Block output;
    int amount = 1, output_x = 0, output_y = 0;
    int width = 0, height = 0;
    std::vector<Ingredient> shape;
    std::vector<std::string> stations;
};

struct Assembly {
    int existing = 0;
    int quantity = 0;
    bool bench = false;
    int setting = 1;
    bool conflicting_benches = false;
    std::set<std::string> stations;
};

// A cell's quantity, for an unfinished ingredient standing for a whole batch, and a workbench's batch.
int property(const Cell& cell, const char* name) {
    if (cell.properties.empty()) return 1;
    return int(Field(cell.properties["crafting"][name], std::string("crafting.") + name).integer_or(1));
}

Cell with(Cell cell, const char* name, int value) {
    Value own = cell.properties["crafting"];
    own.set(name, value == 1 ? Value() : Value(value));
    cell.properties.set("crafting", std::move(own));
    return cell;
}

class Crafting {
public:
    explicit Crafting(Host& host) : host(host) {
        for (const auto& [path, list] : host.content.parts("recipes.json")) read(Field(list, path.string()));
        // Larger, more specific patterns win. Pack load order grants no priority.
        std::ranges::sort(recipes, [](const Recipe& a, const Recipe& b) {
            if (a.shape.size() != b.shape.size()) return a.shape.size() > b.shape.size();
            return a.id < b.id;
        });
        for (const auto& [id, definition] : host.content.blocks) {
            auto station = Field(definition.properties["crafting"]["station"], id + ".crafting.station").text_or("");
            if (!station.empty()) stations[id] = station;
        }
        for (const auto& r : recipes) book.push_back(&r);
        std::ranges::sort(book, {}, &Recipe::order);
    }

    Host& host;
    std::vector<Recipe> recipes;
    std::map<Block, std::string> stations;
    std::vector<const Recipe*> book;
    bool open = false;
    int page = 0;
    bool changing = false; // Crafting's own changes complete nothing.

    const std::string& station(const Block& id) const {
        static const std::string none;
        auto found = stations.find(id);
        return found == stations.end() ? none : found->second;
    }

    int placement_quantity(const Block& ingredient, int x, int y) {
        int progress = -1;
        std::set<int> quantities;
        for (const auto& r : recipes)
            for (auto [ox, oy] : origins(r, x, y, &ingredient)) {
                auto a = assembly(r, ox, oy, false);
                if (!a || (!a->bench && a->existing == 0)) continue;
                int quantity = a->quantity ? a->quantity : (a->conflicting_benches ? 0 : a->setting);
                if (quantity > 1 && !a->bench) quantity = 0;
                if (a->existing > progress) { progress = a->existing; quantities.clear(); }
                if (a->existing == progress) quantities.insert(quantity);
            }
        if (quantities.empty()) return 1;
        return quantities.size() == 1 ? *quantities.begin() : 0;
    }

    void craft(int changed_x, int changed_y) {
        auto& world = host.world();
        for (const auto& r : recipes)
            for (auto [x, y] : origins(r, changed_x, changed_y, nullptr)) {
                auto a = assembly(r, x, y, true);
                if (!a) continue;
                int width = world.settings.width();
                Transformation made{{wrap(x + r.output_x, width), y + r.output_y, r.output}, {}};
                changing = true;
                for (const auto& i : r.shape)
                    if (!i.id.empty()) {
                        world.set(x + i.x, y + i.y, {});
                        made.consumed.push_back({wrap(x + i.x, width), y + i.y, i.id});
                    }
                auto output = std::int64_t(r.amount) * a->quantity;
                world.put(made.result.x, made.result.y, Cell{r.output, {}});
                changing = false;
                // One result stands in the world; only a surplus is carried.
                if (output > 1) inventory::carry(host.player(), r.output, output - 1);
                host.events.announce(Notice{"Made " + host.content.name(r.output) + " x" + std::to_string(output)});
                host.events.announce(std::move(made));
                return;
            }
    }

    // K on a workbench cycles its batch: 1, 5, 10.
    bool use(const Target& t) {
        auto& world = host.world();
        auto cell = world.cell(t.x, t.y, t.layer);
        if (station(cell.id) != "workbench") return false;
        int batch = property(cell, "batch");
        batch = batch == 1 ? 5 : (batch == 5 ? 10 : 1);
        changing = true;
        world.put(t.x, t.y, with(cell, "batch", batch), t.layer);
        changing = false;
        host.events.announce(Notice{"Workbench batch x" + std::to_string(batch)});
        return true;
    }

    Book shown() {
        Book view{.open = open, .toggle = [this] { open = !open; }};
        const auto& r = *book[page];
        view.output = r.output;
        view.amount = r.amount;
        view.width = r.width;
        view.height = r.height;
        std::map<Block, int> needed;
        for (const auto& i : r.shape) {
            view.shape.push_back({i.x, i.y, i.id});
            if (!i.id.empty()) ++needed[i.id];
        }
        // Each required working surface, shown by the first block that is one.
        for (const auto& s : r.stations)
            for (const auto& [id, kind] : stations)
                if (kind == s) { view.stations.push_back(id); break; }
        for (const auto& [id, n] : needed) view.needs.push_back({id, n, inventory::count(host.player(), id) >= n});
        view.index = page;
        view.count = int(book.size());
        view.browse = [this](int pages) { browse(pages); };
        return view;
    }

    void browse(int pages) {
        page = wrap(page + pages, int(book.size()));
    }

private:
    void read(const Field& list) {
        for (std::size_t n = 0; n < list.size(); ++n) {
            auto entry = list[n];
            Recipe r;
            r.id = entry["id"].text();
            auto output = entry["output"];
            auto coordinate = [](const Field& f) { return int(f.integer()); };
            r.output = output["block"].text();
            r.amount = int(output["count"].integer_or(1));
            r.output_x = coordinate(output["x"]);
            r.output_y = coordinate(output["y"]);
            // Rows run down and cells run right. A block ID is an ingredient; null must stay empty.
            auto rows = entry["shape"];
            for (std::size_t y = 0; y < rows.size(); ++y)
                for (std::size_t x = 0; x < rows[y].size(); ++x) {
                    auto cell = rows[y][x];
                    r.shape.push_back({int(x), int(y), cell.present() ? cell.text() : Block{}});
                    r.width = std::max(r.width, int(x) + 1);
                    r.height = std::max(r.height, int(y) + 1);
                }
            auto stations = entry["stations"];
            for (std::size_t i = 0; i < stations.size(); ++i) r.stations.push_back(stations[i].text());
            r.order = int(recipes.size());
            recipes.push_back(r);
        }
    }

    // Partial matching is also how a placement finds the workshop it belongs to.
    // Nothing is consumed or reserved until the complete shape matches.
    std::optional<Assembly> assembly(const Recipe& r, int x, int y, bool complete) {
        auto& world = host.world();
        if (y < 0 || y + r.height > world.settings.height()) return {};
        Assembly a;
        for (const auto& i : r.shape) {
            const auto& cell = world.cell(x + i.x, y + i.y);
            if (i.id.empty()) {
                if (!cell.id.empty()) return {};
            } else if (cell.id.empty()) {
                if (complete) return {};
            } else {
                if (cell.id != i.id) return {};
                int quantity = property(cell, "quantity");
                if (a.quantity && a.quantity != quantity) return {};
                a.quantity = quantity;
                ++a.existing;
            }
        }
        // All supports are on the row immediately under the shape. A workbench
        // only contributes when the shape actually spans its column.
        for (int column = 0; column < r.width; ++column) {
            const auto& cell = world.cell(x + column, y + r.height);
            const auto& kind = station(cell.id);
            if (!kind.empty()) a.stations.insert(kind);
            if (kind == "workbench") {
                int batch = property(cell, "batch");
                if (a.bench && a.setting != batch) a.conflicting_benches = true;
                a.bench = true;
                a.setting = batch;
            }
        }
        if (complete) {
            for (const auto& s : r.stations) if (!a.stations.contains(s)) return {};
            if (a.quantity > 1 && !a.bench) return {};
        }
        return a;
    }

    std::vector<std::pair<int, int>> origins(const Recipe& r, int x, int y, const Block* placing) {
        int width = host.world().settings.width();
        std::vector<std::pair<int, int>> result;
        for (const auto& i : r.shape) {
            if (placing && i.id != *placing) continue;
            result.emplace_back(wrap(x - i.x, width), y - i.y);
        }
        if (!placing)
            for (int column = 0; column < r.width; ++column) result.emplace_back(wrap(x - column, width), y - r.height);
        std::ranges::sort(result);
        return result;
    }
};

}

void attach(Host& host) {
    auto& crafting = host.adopt<Crafting>(host);
    auto& events = host.events;
    // How many blocks a placement takes: a workbench's batch, or the quantity of the arrangement
    // it joins. Crafting reads the foreground only.
    events.on<Placing>([&crafting](Placing& placing) {
        if (placing.layer != 0) return;
        int quantity = crafting.placement_quantity(placing.cell.id, placing.x, placing.y);
        if (quantity == 0) placing.refusal = "Check the workbench and ingredient batch sizes";
        else {
            placing.count = quantity;
            placing.cell = with(placing.cell, "quantity", quantity);
        }
    });
    events.on<Taking>([](Taking& taking) { taking.count = property(taking.cell, "quantity"); });
    events.on<Changed>([&crafting](Changed& changed) {
        if (!crafting.changing && changed.layer == 0) crafting.craft(changed.x, changed.y);
    });
    events.on<Action>([&crafting](Action& action) {
        if (action.act != Act::b || !crafting.use(action.cell)) return;
        action.claimed = true;
    });
    events.on<Intent>([&crafting](Intent& intent) {
        if (intent.name == "recipes") crafting.open = intent.show || !crafting.open;
        else if (intent.name == "previous page" && crafting.open) crafting.browse(-1);
        else if (intent.name == "next page" && crafting.open) crafting.browse(1);
    });
    host.provide<Book>([&crafting] { return crafting.shown(); });
    host.provide<Label>([] {
        return Label{[](const Cell& cell) {
            int quantity = property(cell, "quantity");
            return quantity > 1 ? std::to_string(quantity) : std::string();
        }};
    });
}

}
