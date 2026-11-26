#pragma once

// What modules hand the engine to keep, and how they read what it kept or read for them.
// The engine does the parsing; nothing outside it sees JSON text.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace yonder {

// A value with JSON's shapes: nothing, true or false, a number, text, a list or named members.
class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::vector<std::pair<std::string, Value>>;

    Value() = default;
    Value(bool b) : data(b) {}
    Value(int n) : data(std::int64_t(n)) {}
    Value(long n) : data(std::int64_t(n)) {}
    // Kept as a signed number whenever it fits, so equal numbers compare equal.
    Value(unsigned long n);
    Value(double d) : data(d) {}
    Value(std::string s) : data(std::move(s)) {}
    Value(const char* s) : data(std::string(s)) {}
    Value(Array a) : data(std::move(a)) {}
    Value(Object o) : data(std::move(o)) {}

    bool null() const { return std::holds_alternative<std::monostate>(data); }
    const Array* array() const { return std::get_if<Array>(&data); }
    const Object* object() const { return std::get_if<Object>(&data); }
    // A member of an object; nothing if absent or if this is not an object.
    const Value& operator[](std::string_view key) const;
    // Sets a member, making this an object if it was nothing. Setting nothing removes it.
    void set(std::string_view key, Value value);
    bool operator==(const Value&) const = default;

    std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string, Array, Object> data;
};

// A value being read, named in errors by where it came from, such as a file and a property.
// Absent and null are the same: an optional property takes its stated default.
class Field {
public:
    Field(const Value& value, std::string where) : node(&value), where(std::move(where)) {}
    bool present() const { return !node->null(); }
    const Value& value() const { return *node; }
    Field operator[](const char* key) const;
    Field operator[](std::size_t index) const;
    std::size_t size() const;              // An array's length; absent is empty.
    std::vector<std::string> keys() const; // An object's property names in order; absent is none.
    std::string text() const;
    std::string text_or(const std::string& fallback) const { return present() ? text() : fallback; }
    std::int64_t integer() const;
    std::int64_t integer_or(std::int64_t fallback) const { return present() ? integer() : fallback; }
    std::uint64_t whole() const;
    double number() const;
    bool flag_or(bool fallback) const;
    [[noreturn]] void fail(const std::string& what) const;

private:
    const Value* node;
    std::string where;
};

// The properties modules own on one thing, each module's group under its name. The engine keeps
// them without reading them, including those of modules the game does not have.
class Properties {
public:
    // A module's group; nothing if it has none.
    const Value& operator[](std::string_view module) const;
    // Replaces a module's group. Nothing, or an object with no members, removes it.
    void set(std::string_view module, Value group);
    bool empty() const { return groups.empty(); }
    bool operator==(const Properties&) const = default;
    std::vector<std::pair<std::string, Value>> groups;
};

}
