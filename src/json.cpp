#include "json.hpp"

#include <json/reader.h>
#include <charconv>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <memory>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace yonder {

Field Field::operator[](const char* key) const {
    if (present() && !value.isObject()) fail("must be an object");
    return Field(present() ? value[key] : value, where + "." + key, source);
}

Field Field::operator[](Json::ArrayIndex index) const {
    if (!value.isArray() || index >= value.size()) fail("has no item " + std::to_string(index));
    return Field(value[index], where + "[" + std::to_string(index) + "]", source);
}

Json::ArrayIndex Field::size() const {
    if (!present()) return 0;
    if (!value.isArray()) fail("must be an array");
    return value.size();
}

std::vector<std::string> Field::keys() const {
    if (!present()) return {};
    if (!value.isObject()) fail("must be an object");
    return value.getMemberNames();
}

std::string Field::text() const {
    if (!value.isString()) fail("must be text");
    return value.asString();
}

std::int64_t Field::integer(std::int64_t low, std::int64_t high) const {
    if (!value.isInt64()) fail("must be a whole number");
    auto n = value.asInt64();
    if (n < low || n > high) fail("must be from " + std::to_string(low) + " to " + std::to_string(high));
    return n;
}

std::uint64_t Field::whole() const {
    if (!value.isUInt64()) fail("must be a whole number, zero or more");
    return value.asUInt64();
}

double Field::number() const {
    if (!value.isNumeric()) fail("must be a number");
    return value.asDouble();
}

bool Field::flag_or(bool fallback) const {
    if (!present()) return fallback;
    if (!value.isBool()) fail("must be true or false");
    return value.asBool();
}

Unrecognized Field::rest(std::initializer_list<std::string_view> known) const {
    Unrecognized result;
    std::vector<std::pair<std::ptrdiff_t, std::string>> found;
    for (const auto& name : keys())
        if (std::ranges::find(known, std::string_view(name)) == known.end()) found.emplace_back(value[name].getOffsetStart(), name);
    std::ranges::sort(found);
    for (const auto& [start, name] : found) {
        const auto& member = value[name];
        result.members.emplace_back(name, source.substr(start, member.getOffsetLimit() - start));
    }
    return result;
}

void Field::fail(const std::string& what) const {
    throw std::runtime_error(where + " " + what);
}

JsonFile::JsonFile(const std::filesystem::path& file) : name(file.string()) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read " + name);
    std::ostringstream all;
    all << in.rdbuf();
    text = all.str();
    if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3); // An editor's byte order mark.
    // Plain JSON: no comments, trailing commas or repeated keys.
    Json::CharReaderBuilder builder;
    Json::CharReaderBuilder::strictMode(&builder.settings_);
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;
    if (!reader->parse(text.data(), text.data() + text.size(), &value, &errors))
        throw std::runtime_error(name + " is not valid JSON: " + errors);
}

JsonWriter& JsonWriter::open(char bracket, bool flat) {
    separate();
    out += bracket;
    // Nothing inside a one-line container spreads over several.
    levels.push_back({bracket == '{' ? '}' : ']', flat || (!levels.empty() && levels.back().flat)});
    return *this;
}

JsonWriter& JsonWriter::close() {
    auto level = levels.back();
    levels.pop_back();
    if (!level.flat && !level.empty) indent(levels.size());
    out += level.close;
    return *this;
}

void JsonWriter::indent(std::size_t depth) {
    out += '\n';
    out.append(2 * depth, ' ');
}

void JsonWriter::separate() {
    if (after_key) { after_key = false; return; }
    if (levels.empty()) return;
    auto& level = levels.back();
    if (!level.empty) out += level.flat ? ", " : ",";
    if (!level.flat) indent(levels.size());
    level.empty = false;
}

JsonWriter& JsonWriter::key(std::string_view name) {
    text(name);
    out += ": ";
    after_key = true;
    return *this;
}

JsonWriter& JsonWriter::text(std::string_view value) {
    separate();
    out += '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) {
            char code[7];
            std::snprintf(code, sizeof code, "\\u%04x", c);
            out += code;
        } else out += char(c);
    }
    out += '"';
    return *this;
}

JsonWriter& JsonWriter::integer(std::int64_t value) {
    separate();
    out += std::to_string(value);
    return *this;
}

JsonWriter& JsonWriter::whole(std::uint64_t value) {
    separate();
    out += std::to_string(value);
    return *this;
}

JsonWriter& JsonWriter::number(double value) {
    if (!std::isfinite(value)) throw std::runtime_error("Cannot save a number that is not finite");
    separate();
    // The shortest text that reads back as exactly this value.
    char text[32];
    auto end = std::to_chars(text, text + sizeof text, value).ptr;
    out.append(text, end);
    return *this;
}

JsonWriter& JsonWriter::flag(bool value) {
    separate();
    out += value ? "true" : "false";
    return *this;
}

JsonWriter& JsonWriter::null() {
    separate();
    out += "null";
    return *this;
}

JsonWriter& JsonWriter::rest(const Unrecognized& members) {
    for (const auto& [name, json] : members.members) {
        key(name);
        separate();
        out += json;
    }
    return *this;
}

std::string JsonWriter::finish() {
    if (!levels.empty()) throw std::logic_error("Unclosed JSON container");
    return out + '\n';
}

void replace_file(const std::filesystem::path& file, const std::string& text) {
    auto temporary = std::filesystem::path(file.string() + ".tmp");
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << text;
        out.close();
        if (!out) throw std::runtime_error("Cannot write " + temporary.string());
    }
    std::filesystem::rename(temporary, file);
}

namespace {
// Generation roles in their written order. Expedition generations add the last two.
constexpr std::pair<const char*, Block Terrain::*> roles[]{
    {"grass", &Terrain::grass}, {"soil", &Terrain::soil}, {"stone", &Terrain::stone}, {"sand", &Terrain::sand},
    {"timber", &Terrain::timber}, {"leaves", &Terrain::leaves}, {"clay", &Terrain::clay}, {"ore", &Terrain::ore}};
constexpr int shelter_roles = 6;
}

Terrain read_terrain(const Field& field) {
    Terrain t;
    t.version = field["generation"].text();
    int rank = profile_rank(t.version);
    if (rank == 0) field["generation"].fail("is not a generation this build knows: " + t.version);
    auto materials = field["materials"];
    for (int i = 0; i < int(std::size(roles)); ++i) {
        auto [name, role] = roles[i];
        if (i < shelter_roles || rank >= profile_rank("expedition-v1")) t.*role = materials[name].text();
        else t.*role = materials[name].text_or("");
    }
    t.unknown = field.rest({"generation", "materials"});
    t.unknown_materials = materials.rest({"grass", "soil", "stone", "sand", "timber", "leaves", "clay", "ore"});
    return t;
}

void write_terrain(JsonWriter& out, const Terrain& t) {
    out.object().key("generation").text(t.version).key("materials").object();
    for (auto [name, role] : roles)
        if (!(t.*role).empty()) out.key(name).text(t.*role);
    out.rest(t.unknown_materials).close().rest(t.unknown).close();
}

}
