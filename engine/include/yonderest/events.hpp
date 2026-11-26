#pragma once

// Hooks. A pre-hook comes before a change and can refuse or adjust it; an event follows the
// change. Both are announcements of a type, heard by every handler of that type in order.
// An announcement that can be settled early, such as an action a layer has claimed, says so
// through `stopped()`, and later handlers do not hear it.

#include <algorithm>
#include <functional>
#include <map>
#include <typeindex>
#include <utility>
#include <vector>

namespace yonder {

class Events {
public:
    // Handlers hear an announcement in order of stage, then in the order they were added.
    // Checks that depend on adjustments, such as whether enough is carried, use a later stage.
    template <class E>
    void on(std::function<void(E&)> handler, int stage = 0) {
        auto& list = handlers[std::type_index(typeid(E))];
        auto at = std::ranges::upper_bound(list, stage, {}, &Entry::stage);
        list.insert(at, {stage, [handler = std::move(handler)](void* event) { handler(*static_cast<E*>(event)); }});
    }

    template <class E>
    E& announce(E& event) {
        auto found = handlers.find(std::type_index(typeid(E)));
        if (found == handlers.end()) return event;
        for (const auto& entry : found->second) {
            entry.call(&event);
            if constexpr (requires { event.stopped(); })
                if (event.stopped()) break;
        }
        return event;
    }
    template <class E>
    E announce(E&& event) {
        announce(event);
        return std::move(event);
    }

private:
    struct Entry {
        int stage;
        std::function<void(void*)> call;
    };
    std::map<std::type_index, std::vector<Entry>> handlers;
};

}
