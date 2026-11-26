#include "yonderest/value.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace yonder {

Value::Value(unsigned long n) {
    if (n <= (unsigned long)std::numeric_limits<std::int64_t>::max()) data = std::int64_t(n);
    else data = std::uint64_t(n);
}

const Value& Value::operator[](std::string_view key) const {
    static const Value nothing;
    if (const auto* members = object())
        for (const auto& [name, value] : *members)
            if (name == key) return value;
    return nothing;
}

void Value::set(std::string_view key, Value value) {
    if (null()) data = Object{};
    auto& members = std::get<Object>(data);
    auto found = std::ranges::find(members, key, &Object::value_type::first);
    if (value.null()) {
        if (found != members.end()) members.erase(found);
    } else if (found != members.end()) found->second = std::move(value);
    else members.emplace_back(std::string(key), std::move(value));
}

Field Field::operator[](const char* key) const {
    if (present() && !node->object()) fail("must be an object");
    return Field((*node)[key], where + "." + key);
}

Field Field::operator[](std::size_t index) const {
    const auto* items = node->array();
    if (!items || index >= items->size()) fail("has no item " + std::to_string(index));
    return Field((*items)[index], where + "[" + std::to_string(index) + "]");
}

std::size_t Field::size() const {
    if (!present()) return 0;
    const auto* items = node->array();
    if (!items) fail("must be an array");
    return items->size();
}

std::vector<std::string> Field::keys() const {
    if (!present()) return {};
    const auto* members = node->object();
    if (!members) fail("must be an object");
    std::vector<std::string> names;
    for (const auto& [name, value] : *members) names.push_back(name);
    return names;
}

std::string Field::text() const {
    const auto* s = std::get_if<std::string>(&node->data);
    if (!s) fail("must be text");
    return *s;
}

std::int64_t Field::integer() const {
    const auto* n = std::get_if<std::int64_t>(&node->data);
    if (!n) fail("must be a whole number");
    return *n;
}

std::uint64_t Field::whole() const {
    if (const auto* n = std::get_if<std::int64_t>(&node->data); n && *n >= 0) return std::uint64_t(*n);
    if (const auto* n = std::get_if<std::uint64_t>(&node->data)) return *n;
    fail("must be a whole number, zero or more");
}

double Field::number() const {
    if (const auto* n = std::get_if<std::int64_t>(&node->data)) return double(*n);
    if (const auto* n = std::get_if<std::uint64_t>(&node->data)) return double(*n);
    if (const auto* n = std::get_if<double>(&node->data)) return *n;
    fail("must be a number");
}

bool Field::flag_or(bool fallback) const {
    if (!present()) return fallback;
    const auto* b = std::get_if<bool>(&node->data);
    if (!b) fail("must be true or false");
    return *b;
}

void Field::fail(const std::string& what) const {
    throw std::runtime_error(where + " " + what);
}

const Value& Properties::operator[](std::string_view module) const {
    static const Value nothing;
    auto found = std::ranges::find(groups, module, &std::pair<std::string, Value>::first);
    return found == groups.end() ? nothing : found->second;
}

void Properties::set(std::string_view module, Value group) {
    auto found = std::ranges::find(groups, module, &std::pair<std::string, Value>::first);
    bool empty = group.null() || (group.object() && group.object()->empty());
    if (empty) {
        if (found != groups.end()) groups.erase(found);
    } else if (found != groups.end()) found->second = std::move(group);
    else groups.emplace_back(std::string(module), std::move(group));
}

}
