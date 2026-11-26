#pragma once

// JSON stays at the edges. A file is read into values where it is loaded and written from them
// where it is saved; nothing outside the engine handles JSON.

#include "yonderest/value.hpp"

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yonder {

// A parsed file. It remembers where each object member's value lay in the text, so that
// properties can be kept as they were written.
class JsonFile {
public:
    explicit JsonFile(const std::filesystem::path& file);
    Field root() const { return Field(value, name); }
    // An object's members other than `known`, in file order, as properties.
    Properties rest(const Field& object, std::initializer_list<std::string_view> known) const;

private:
    std::string text, name;
    Value value;
    std::map<const Value*, std::pair<std::size_t, std::size_t>> spans;
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
    // A list of plain values shares one line; anything else spreads over several.
    JsonWriter& value(const Value& value);
    // Each module's group under its name, as it was read until it changed.
    JsonWriter& properties(const Properties& properties);
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

}
