#include "json.hpp"
#include "yonderest/content.hpp"

#include <json/reader.h>
#include <json/value.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace yonder {
namespace {

// An object's member names in the order they were written; JsonCpp keeps them sorted.
std::vector<std::string> in_order(const Json::Value& object) {
    auto names = object.getMemberNames();
    std::ranges::sort(names, {}, [&](const std::string& name) { return object[name].getOffsetStart(); });
    return names;
}

Value convert(const Json::Value& json) {
    switch (json.type()) {
    case Json::nullValue: return {};
    case Json::booleanValue: return json.asBool();
    case Json::intValue: return std::int64_t(json.asInt64());
    case Json::uintValue: return (unsigned long)json.asUInt64();
    case Json::realValue: return json.asDouble();
    case Json::stringValue: return json.asString();
    case Json::arrayValue: {
        Value::Array items;
        items.reserve(json.size());
        for (const auto& item : json) items.push_back(convert(item));
        return items;
    }
    case Json::objectValue: {
        Value::Object members;
        for (const auto& name : in_order(json)) members.emplace_back(name, convert(json[name]));
        return members;
    }
    }
    return {};
}

}

Value read_json(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    auto text = all.str();
    if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3); // An editor's byte order mark.
    // Plain JSON: no comments, trailing commas or repeated keys.
    Json::CharReaderBuilder builder;
    Json::CharReaderBuilder::strictMode(&builder.settings_);
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value json;
    std::string errors;
    if (!reader->parse(text.data(), text.data() + text.size(), &json, &errors))
        throw std::runtime_error(file.string() + " is not valid JSON: " + errors);
    return convert(json);
}

Properties rest(const Value& object, std::initializer_list<std::string_view> known) {
    Properties result;
    if (const auto* members = object.object())
        for (const auto& [name, value] : *members)
            if (std::ranges::find(known, std::string_view(name)) == known.end()) result.groups.emplace_back(name, value);
    return result;
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

JsonWriter& JsonWriter::value(const Value& v) {
    struct {
        JsonWriter& out;
        void operator()(std::monostate) { out.null(); }
        void operator()(bool b) { out.flag(b); }
        void operator()(std::int64_t n) { out.integer(n); }
        void operator()(std::uint64_t n) { out.whole(n); }
        void operator()(double d) { out.number(d); }
        void operator()(const std::string& s) { out.text(s); }
        void operator()(const Value::Array& items) {
            bool plain = std::ranges::none_of(items, [](const Value& i) { return i.array() || i.object(); });
            out.array(plain);
            for (const auto& item : items) out.value(item);
            out.close();
        }
        void operator()(const Value::Object& members) {
            out.object();
            for (const auto& [name, member] : members) out.key(name).value(member);
            out.close();
        }
    } visit{*this};
    std::visit(visit, v.data);
    return *this;
}

JsonWriter& JsonWriter::properties(const Properties& properties) {
    for (const auto& [name, group] : properties.groups) key(name).value(group);
    return *this;
}

std::string JsonWriter::finish() {
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

}
