#pragma once

// JSON stays at the edges. A file is read into the game's own data where it is loaded and
// written from that data where it is saved; nothing else in the game handles JSON.

#include "world.hpp"

#include <json/value.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace yonder {

// A value read from a file, with errors that name the file and the property.
// Absent and null are the same: an optional property takes its stated default.
class Field {
public:
    Field(const Json::Value& value, std::string where, const std::string& source)
        : value(value), where(std::move(where)), source(source) {}
    bool present() const { return !value.isNull(); }
    Field operator[](const char* key) const;
    Field operator[](Json::ArrayIndex index) const;
    Json::ArrayIndex size() const;                // An array's length; absent is empty.
    std::vector<std::string> keys() const;        // An object's property names; absent is none.
    std::string text() const;
    std::string text_or(const std::string& fallback) const { return present() ? text() : fallback; }
    std::int64_t integer(std::int64_t low, std::int64_t high) const;
    std::int64_t integer_or(std::int64_t fallback, std::int64_t low, std::int64_t high) const {
        return present() ? integer(low, high) : fallback;
    }
    std::uint64_t whole() const;
    double number() const;
    bool flag_or(bool fallback) const;
    // An object's other properties, in file order, as their original text.
    Unrecognized rest(std::initializer_list<std::string_view> known) const;
    [[noreturn]] void fail(const std::string& what) const;

private:
    const Json::Value& value;
    std::string where;
    const std::string& source;
};

// Owns a parsed file; `root()` reads it.
class JsonFile {
public:
    explicit JsonFile(const std::filesystem::path& file);
    Field root() const { return Field(value, name, text); }

private:
    std::string text;
    Json::Value value;
    std::string name;
};

// Writes JSON in the order given. Members of a container opened `flat` share one line,
// so a row of a chunk reads as a row; other containers put each member on its own line.
class JsonWriter {
public:
    JsonWriter& object(bool flat = false) { return open('{', flat); }
    JsonWriter& array(bool flat = false) { return open('[', flat); }
    JsonWriter& close();
    JsonWriter& key(std::string_view name);
    JsonWriter& text(std::string_view value);
    JsonWriter& integer(std::int64_t value);
    JsonWriter& whole(std::uint64_t value);
    JsonWriter& number(double value);
    JsonWriter& flag(bool value);
    JsonWriter& null();
    // Properties kept from a file, written back as they were read.
    JsonWriter& rest(const Unrecognized& members);
    std::string finish();

private:
    struct Level { char close; bool flat, empty = true; };
    std::vector<Level> levels;
    std::string out;
    bool after_key = false;
    JsonWriter& open(char bracket, bool flat);
    void separate();
    void indent(std::size_t depth);
};

// Replaces a file whole: a reader finds the old text or the new, never a mixture.
void replace_file(const std::filesystem::path& file, const std::string& text);

// The terrain object shared by a terrain pack and a saved world.
Terrain read_terrain(const Field& terrain);
void write_terrain(JsonWriter& out, const Terrain& terrain);

}
