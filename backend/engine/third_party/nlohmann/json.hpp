// Minimal JSON stub — compatible with GCC 6.3 / MinGW
// Uses std::map (not unordered_map) to avoid incomplete-type issues.
// Replace with real nlohmann/json.hpp for production use.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <istream>
#include <cstdint>

namespace nlohmann {

class json {
public:
    enum class value_t { null, object, array, string, number_float, boolean };

    json() : type_(value_t::null), num_(0), boolean_(false) {}
    json(const json&) = default;
    json& operator=(const json&) = default;

    // ── Construct from primitives ──────────────────────────────────────────
    json(int v)               : type_(value_t::number_float), num_(v),         boolean_(false) {}
    json(int64_t v)           : type_(value_t::number_float), num_((double)v), boolean_(false) {}
    json(uint64_t v)          : type_(value_t::number_float), num_((double)v), boolean_(false) {}
    json(double v)            : type_(value_t::number_float), num_(v),         boolean_(false) {}
    json(const char* v)       : type_(value_t::string),       num_(0),         boolean_(false), str_(v) {}
    json(const std::string& v): type_(value_t::string),       num_(0),         boolean_(false), str_(v) {}
    json(bool v)              : type_(value_t::boolean),      num_(0),         boolean_(v) {}

    // ── Assignment from primitives ─────────────────────────────────────────
    json& operator=(int v)                { type_=value_t::number_float; num_=v;         return *this; }
    json& operator=(int64_t v)            { type_=value_t::number_float; num_=(double)v; return *this; }
    json& operator=(uint64_t v)           { type_=value_t::number_float; num_=(double)v; return *this; }
    json& operator=(double v)             { type_=value_t::number_float; num_=v;         return *this; }
    json& operator=(const char* v)        { type_=value_t::string;       str_=v;         return *this; }
    json& operator=(const std::string& v) { type_=value_t::string;       str_=v;         return *this; }
    json& operator=(bool v)               { type_=value_t::boolean;      boolean_=v;     return *this; }

    // ── Conversions — explicit only to avoid ambiguity ─────────────────────
    explicit operator int()         const { return (int)num_; }
    explicit operator int64_t()     const { return (int64_t)num_; }
    explicit operator uint64_t()    const { return (uint64_t)num_; }
    explicit operator std::string() const { return str_; }
    explicit operator bool()        const { return boolean_; }
    // Non-explicit double (used for arithmetic and unambiguous assignments)
    operator double() const { return num_; }

    // ── Array factory ──────────────────────────────────────────────────────
    static json array() { json j; j.type_ = value_t::array; return j; }

    // ── Accessors ──────────────────────────────────────────────────────────
    bool contains(const std::string& k) const {
        return type_ == value_t::object && obj_.count(k) > 0;
    }
    const json& at(const std::string& k) const {
        auto it = obj_.find(k);
        if (it == obj_.end()) throw std::runtime_error("Key not found: " + k);
        return it->second;
    }
    const json& at(size_t i) const {
        if (i >= arr_.size()) throw std::out_of_range("Array index out of bounds");
        return arr_[i];
    }
    const json& operator[](const std::string& k) const { return at(k); }
    json& operator[](const std::string& k) { type_ = value_t::object; return obj_[k]; }
    json& operator[](const char* k)        { return (*this)[std::string(k)]; }

    // ── Array iteration ────────────────────────────────────────────────────
    std::vector<json>::const_iterator begin() const { return arr_.begin(); }
    std::vector<json>::const_iterator end()   const { return arr_.end(); }
    size_t size() const {
        if (type_==value_t::array)  return arr_.size();
        if (type_==value_t::object) return obj_.size();
        return 0;
    }
    void push_back(const json& v) { type_=value_t::array; arr_.push_back(v); }

    // ── Serialize ──────────────────────────────────────────────────────────
    std::string dump(int = -1) const {
        std::ostringstream os;
        write(os);
        return os.str();
    }

    // ── Parse ──────────────────────────────────────────────────────────────
    static json parse(const std::string& s) {
        size_t pos = 0;
        return parseVal(s, pos);
    }
    friend std::istream& operator>>(std::istream& is, json& j) {
        std::string s((std::istreambuf_iterator<char>(is)), {});
        j = json::parse(s);
        return is;
    }

// ── Internal storage ────────────────────────────────────────────────────────
    value_t                   type_;
    double                    num_;
    bool                      boolean_;
    std::string               str_;
    std::map<std::string,json> obj_;
    std::vector<json>          arr_;

private:
    void write(std::ostringstream& os) const {
        switch (type_) {
            case value_t::null:         os << "null"; break;
            case value_t::boolean:      os << (boolean_ ? "true" : "false"); break;
            case value_t::number_float:
                if (num_ == (int64_t)num_) os << (int64_t)num_;
                else os << num_;
                break;
            case value_t::string: {
                os << '"';
                for (char c : str_) {
                    if (c=='"') os << "\\\"";
                    else if (c=='\\') os << "\\\\";
                    else if (c=='\n') os << "\\n";
                    else os << c;
                }
                os << '"';
                break;
            }
            case value_t::array: {
                os << '[';
                for (size_t i = 0; i < arr_.size(); ++i) {
                    if (i) os << ',';
                    arr_[i].write(os);
                }
                os << ']';
                break;
            }
            case value_t::object: {
                os << '{';
                bool first = true;
                for (auto& kv : obj_) {
                    if (!first) os << ',';
                    first = false;
                    os << '"' << kv.first << "\":";
                    kv.second.write(os);
                }
                os << '}';
                break;
            }
        }
    }

    static void skipWs(const std::string& s, size_t& p) {
        while (p < s.size() && isspace((unsigned char)s[p])) ++p;
    }
    static std::string parseStr(const std::string& s, size_t& p) {
        ++p;
        std::string r;
        while (p < s.size() && s[p] != '"') {
            if (s[p]=='\\') { ++p; r += (s[p]=='n'?'\n':s[p]=='t'?'\t':s[p]); }
            else r += s[p];
            ++p;
        }
        ++p;
        return r;
    }
    static json parseVal(const std::string& s, size_t& p) {
        skipWs(s, p);
        if (p >= s.size()) return json();
        char c = s[p];
        if (c == '{') {
            json j; j.type_ = value_t::object; ++p;
            skipWs(s, p);
            while (p < s.size() && s[p] != '}') {
                skipWs(s, p);
                std::string key = parseStr(s, p);
                skipWs(s, p); ++p; // ':'
                j.obj_[key] = parseVal(s, p);
                skipWs(s, p);
                if (p < s.size() && s[p] == ',') ++p;
                skipWs(s, p);
            }
            ++p;
            return j;
        }
        if (c == '[') {
            json j; j.type_ = value_t::array; ++p;
            skipWs(s, p);
            while (p < s.size() && s[p] != ']') {
                j.arr_.push_back(parseVal(s, p));
                skipWs(s, p);
                if (p < s.size() && s[p] == ',') ++p;
                skipWs(s, p);
            }
            ++p;
            return j;
        }
        if (c == '"') { json j; j.type_=value_t::string; j.str_=parseStr(s,p); return j; }
        if (c == 't') { p+=4; json j; j.type_=value_t::boolean; j.boolean_=true;  return j; }
        if (c == 'f') { p+=5; json j; j.type_=value_t::boolean; j.boolean_=false; return j; }
        if (c == 'n') { p+=4; return json(); }
        {
            size_t start = p;
            if (s[p]=='-') ++p;
            while (p<s.size() && (isdigit((unsigned char)s[p])||s[p]=='.'||s[p]=='e'||s[p]=='E'||s[p]=='+'||s[p]=='-')) ++p;
            json j; j.type_=value_t::number_float; j.num_=std::stod(s.substr(start,p-start)); return j;
        }
    }
};

} // namespace nlohmann
