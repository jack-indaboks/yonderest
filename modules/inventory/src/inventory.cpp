// The inventory: what the player carries, holds and readies. H moves the hand from a held object
// to the empty hand, to the ring of readied objects, to holding the ring's choice; the backpack
// screen shows everything carried. The carried objects are kept with the player.

#include "yonderest/inventory.hpp"
#include "yonderest/host.hpp"

#include <algorithm>
#include <map>
#include <vector>

namespace yonder::inventory {
namespace {

// Readied stacks, shown as a ring around the character.
constexpr std::size_t ring_size = 12;

struct Pack {
    std::map<Block, std::int64_t> carried;
    std::vector<Block> ready;
    Block last_held;
};

Pack read(const Player& player) {
    Field own(player.properties["inventory"], "player.inventory");
    Pack pack;
    pack.last_held = own["last_held"].text_or("");
    auto carried = own["carried"];
    for (const auto& id : carried.keys())
        pack.carried[id] = carried[id.c_str()].integer();
    auto ready = own["ready"];
    for (std::size_t i = 0; i < ready.size(); ++i) pack.ready.push_back(ready[i].text());
    return pack;
}

void write(Player& player, const Pack& pack) {
    Value own = player.properties["inventory"];
    own.set("last_held", pack.last_held.empty() ? Value() : Value(pack.last_held));
    Value::Object carried;
    for (const auto& [id, count] : pack.carried) carried.emplace_back(id, Value(count));
    own.set("carried", carried.empty() ? Value() : Value(std::move(carried)));
    Value::Array ready(pack.ready.begin(), pack.ready.end());
    own.set("ready", ready.empty() ? Value() : Value(std::move(ready)));
    player.properties.set("inventory", std::move(own));
}

class Inventory;

class BackpackScreen : public Screen {
public:
    explicit BackpackScreen(Inventory& inventory) : inventory(inventory) {}
    static constexpr int columns = 8, rows = 4;
    int index = 0, focus = -1;
    std::string_view name() const override { return "backpack"; }
    void command(std::string_view command) override;
    View view() override;
    void act(int action);

private:
    Inventory& inventory;
};

class Inventory {
public:
    explicit Inventory(Host& host) : host(host), backpack(*this) {}
    Host& host;
    BackpackScreen backpack;
    bool ring = false; // The ring of readied objects is open.
    Block choice;

    Player& player() { return host.player(); }

    std::vector<Block> slots() {
        std::vector<Block> result;
        for (const auto& [id, count] : read(player()).carried) result.push_back(id);
        return result;
    }

    void equip(const Block& id) {
        ring = false;
        host.hold(id);
        if (!id.empty()) {
            auto pack = read(player());
            pack.last_held = id;
            write(player(), pack);
        }
    }

    void hand() {
        auto& p = player();
        auto pack = read(p);
        if (!p.held.empty()) {
            pack.last_held = p.held;
            write(p, pack);
            host.hold({});
        } else if (!ring) {
            ring = true;
            choice = std::ranges::find(pack.ready, pack.last_held) != pack.ready.end() ? pack.last_held
                : (pack.ready.empty() ? Block{} : pack.ready.front());
        } else equip(choice);
    }

    void cycle(int direction) {
        auto ready = read(player()).ready;
        if (ready.empty()) return;
        int index = int(std::ranges::find(ready, choice) - ready.begin());
        choice = ready[wrap(index + direction, int(ready.size()))];
    }

    void mark(const Block& id) {
        auto pack = read(player());
        auto found = std::ranges::find(pack.ready, id);
        std::string message;
        if (found != pack.ready.end()) {
            pack.ready.erase(found);
            message = "Unreadied";
        } else if (pack.ready.size() == ring_size) message = "Twelve objects are already readied";
        else {
            pack.ready.push_back(id);
            message = "Readied";
        }
        write(player(), pack);
        host.events.announce(Notice{message});
    }

    // Only carried objects exist for the player: an emptied entry leaves the inventory, its
    // readiness and the hand.
    void drop_empty() {
        auto& p = player();
        auto pack = read(p);
        std::erase_if(pack.carried, [](const auto& entry) { return entry.second <= 0; });
        std::erase_if(pack.ready, [&](const Block& id) { return !pack.carried.contains(id); });
        bool emptied = !p.held.empty() && !pack.carried.contains(p.held);
        if (emptied) pack.last_held = p.held;
        write(p, pack);
        if (emptied) host.hold({});
    }

    Ring shown_ring() {
        auto pack = read(player());
        Ring view{true, {}, std::size_t(std::ranges::find(pack.ready, choice) - pack.ready.begin())};
        for (const auto& id : pack.ready) view.entries.emplace_back(id, pack.carried[id]);
        return view;
    }
};

void BackpackScreen::command(std::string_view command) {
    if (command == "close" || command == "back") return inventory.host.close();
    if (command == "empty hand") return act(2);
    auto items = inventory.slots();
    if (items.empty()) return;
    int dx = (command == "right") - (command == "left");
    int dy = (command == "down") - (command == "up");
    if (focus < 0) {
        int next = index + dx + dy * columns;
        // Down from the last row reaches the actions.
        if (dy > 0 && next >= int(items.size())) focus = 0;
        else index = std::clamp(next, 0, int(items.size()) - 1);
    } else {
        if (dy < 0) focus = -1;
        else focus = std::clamp(focus + dx, 0, 2);
    }
    if (command == "confirm") act(focus < 0 ? 0 : focus);
    if (command == "ready") act(1);
    if (command == "hold") act(0);
}

// Hold, ready or unready, or empty the hand.
void BackpackScreen::act(int action) {
    auto items = inventory.slots();
    if (action == 2) {
        inventory.equip({});
        return inventory.host.close();
    }
    if (items.empty()) return;
    const auto& id = items[index];
    if (action == 0) {
        inventory.equip(id);
        inventory.host.close();
    } else inventory.mark(id);
}

View BackpackScreen::view() {
    auto& p = inventory.player();
    auto pack = read(p);
    yonder::Backpack view;
    view.title = "Inventory";
    view.readiness = "Ready " + std::to_string(pack.ready.size()) + "/12";
    view.columns = columns;
    view.rows = rows;
    view.index = index;
    view.focus = focus;
    for (const auto& [id, count] : pack.carried) {
        const auto& name = inventory.host.content.name(id);
        bool ready = std::ranges::find(pack.ready, id) != pack.ready.end();
        view.entries.push_back({id, name.empty() ? "Unknown" : name, count, ready, id == p.held});
    }
    bool ready = !view.entries.empty() && view.entries[std::min(index, int(view.entries.size()) - 1)].ready;
    view.actions = {"Hold  K", ready ? "Unready J" : "Ready  J", "Empty hand  H"};
    view.select = [this](int entry, int clicks) {
        index = entry;
        focus = -1;
        if (clicks == 2) act(0);
    };
    view.act = [this](int action) { act(action); };
    return view;
}

}

std::int64_t count(const Player& player, const Block& block) {
    auto pack = read(player);
    auto found = pack.carried.find(block);
    return found == pack.carried.end() ? 0 : found->second;
}

void carry(Player& player, const Block& block, std::int64_t count) {
    auto pack = read(player);
    if (!pack.carried.contains(block) && pack.ready.size() < ring_size) pack.ready.push_back(block);
    pack.carried[block] += count;
    write(player, pack);
}

void attach(Host& host) {
    auto& inventory = host.adopt<Inventory>(host);
    auto& events = host.events;
    events.on<Started>([&inventory](Started&) { inventory.drop_empty(); });
    events.on<Intent>([&inventory, &host](Intent& intent) {
        if (intent.name == "hand") inventory.hand();
        else if (intent.name == "backpack") {
            inventory.backpack.index = 0;
            inventory.backpack.focus = -1;
            host.open(inventory.backpack);
        } else if (intent.name == "recipes" && host.screen() == &inventory.backpack) {
            // The backpack makes way for the book.
            host.close();
            intent.show = true;
        }
    });
    // While the ring is open, the actions step through it instead of acting on the world.
    events.on<Action>([&inventory](Action& action) {
        if (!inventory.ring || action.act == Act::c) return;
        inventory.cycle(action.act == Act::a ? -1 : 1);
        action.claimed = true;
    });
    events.on<Back>([&inventory](Back& back) {
        if (!inventory.ring) return;
        inventory.ring = false;
        back.claimed = true;
    });
    events.on<Interrupted>([&inventory](Interrupted&) { inventory.ring = false; });
    events.on<Placing>([](Placing& placing) {
        if (count(placing.player, placing.cell.id) < placing.count)
            placing.refusal = "Need " + std::to_string(placing.count) + " of this material";
    }, 1);
    events.on<Placed>([&inventory](Placed& placed) {
        auto pack = read(placed.player);
        pack.carried[placed.block] -= placed.count;
        write(placed.player, pack);
        inventory.drop_empty();
    });
    events.on<Taken>([](Taken& taken) { carry(taken.player, taken.block, taken.count); });
    host.provide<Ring>([&inventory]() -> Ring {
        if (!inventory.ring) return {};
        return inventory.shown_ring();
    });
    host.provide<Bag>([&inventory, &host] {
        return Bag{host.screen() == &inventory.backpack, [&inventory, &host] {
            if (host.screen() == &inventory.backpack) host.close();
            else {
                inventory.backpack.index = 0;
                inventory.backpack.focus = -1;
                host.open(inventory.backpack);
            }
        }};
    });
}

}
