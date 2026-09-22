#pragma once
// Offline, bounded JSON only. Numbers retain their lexeme; IDs never pass
// through double.
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace tracecli {
struct Problem : std::runtime_error {
  std::string code, record, field;
  int exitCode;
  Problem(std::string c, std::string m, std::string r = {}, std::string f = {},
          int e = 2)
      : std::runtime_error(m), code(std::move(c)), record(std::move(r)),
        field(std::move(f)), exitCode(e) {}
};
struct Json {
  enum Type { Null, Bool, Number, String, Array, Object } type = Null;
  std::string value;
  std::vector<Json> array;
  std::map<std::string, Json> object;
  Json() = default;
  Json(const char *s) : type(String), value(s) {}
  Json(std::string s) : type(String), value(std::move(s)) {}
  Json(bool b) : type(Bool), value(b ? "true" : "false") {}
  static Json number(uint64_t n) {
    Json j;
    j.type = Number;
    j.value = std::to_string(n);
    return j;
  }
  static Json list() {
    Json j;
    j.type = Array;
    return j;
  }
  static Json
  obj(std::initializer_list<std::pair<const std::string, Json>> v = {}) {
    Json j;
    j.type = Object;
    j.object = v;
    return j;
  }
  const Json &at(const std::string &k) const {
    auto i = object.find(k);
    if (type != Object || i == object.end())
      throw Problem("TraceInvalidSchema", "Missing key", {}, k);
    return i->second;
  }
  Json &at(const std::string &k) {
    return const_cast<Json &>(static_cast<const Json &>(*this).at(k));
  }
  std::string str() const {
    if (type != String)
      throw Problem("TraceInvalidSchema", "Expected string");
    return value;
  }
  bool yes() const {
    if (type != Bool)
      throw Problem("TraceInvalidSchema", "Expected boolean");
    return value == "true";
  }
  bool null() const { return type == Null; }
  bool operator==(const Json &b) const {
    if (type == Number && b.type == Number)
      return std::stod(value) == std::stod(b.value);
    return type == b.type && value == b.value && array == b.array &&
           object == b.object;
  }
  bool operator!=(const Json &b) const { return !(*this == b); }
};
inline std::string quote(const std::string &s) {
  std::ostringstream o;
  o << '"';
  for (unsigned char c : s) {
    if (c == '"' || c == '\\')
      o << '\\' << c;
    else if (c < 32)
      o << "\\u" << std::hex << std::setw(4) << std::setfill('0')
        << unsigned(c);
    else
      o << c;
  }
  o << '"';
  return o.str();
}
inline std::string dump(const Json &j) {
  if (j.type == Json::Null)
    return "null";
  if (j.type == Json::String)
    return quote(j.value);
  if (j.type == Json::Bool || j.type == Json::Number)
    return j.value;
  std::string s = j.type == Json::Array ? "[" : "{";
  bool first = true;
  if (j.type == Json::Array) {
    for (const auto &v : j.array) {
      if (!first)
        s += ',';
      first = false;
      s += dump(v);
    }
  } else
    for (const auto &p : j.object) {
      if (!first)
        s += ',';
      first = false;
      s += quote(p.first) + ":" + dump(p.second);
    }
  return s + (j.type == Json::Array ? "]" : "}");
}
class Parser {
  const std::string &s;
  size_t p = 0;
  [[noreturn]] void bad() const {
    throw Problem("TraceInvalidSchema",
                  "Invalid JSON at byte " + std::to_string(p));
  }
  void ws() {
    while (p < s.size() &&
           (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n'))
      ++p;
  }
  unsigned hex() {
    unsigned n = 0;
    for (int i = 0; i < 4; ++i) {
      if (p == s.size())
        bad();
      char c = s[p++];
      n *= 16;
      if (c >= '0' && c <= '9')
        n += c - '0';
      else if (c >= 'a' && c <= 'f')
        n += c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        n += c - 'A' + 10;
      else
        bad();
    }
    return n;
  }
  void utf(std::string &out, unsigned c) {
    if (c < 128)
      out += char(c);
    else if (c < 2048) {
      out += char(0xc0 | (c >> 6));
      out += char(0x80 | (c & 63));
    } else if (c < 65536) {
      out += char(0xe0 | (c >> 12));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    } else {
      out += char(0xf0 | (c >> 18));
      out += char(0x80 | ((c >> 12) & 63));
      out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    }
  }
  std::string string() {
    if (p == s.size() || s[p++] != '"')
      bad();
    std::string out;
    while (p < s.size()) {
      unsigned char c = s[p++];
      if (c == '"')
        return out;
      if (c < 32)
        bad();
      if (c == '\\') {
        if (p == s.size())
          bad();
        char e = s[p++];
        switch (e) {
        case '"':
        case '\\':
        case '/':
          out += e;
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'u': {
          unsigned n = hex();
          if (n >= 0xd800 && n <= 0xdbff) {
            if (p + 2 > s.size() || s[p++] != '\\' || s[p++] != 'u')
              bad();
            unsigned low = hex();
            if (low < 0xdc00 || low > 0xdfff)
              bad();
            n = 0x10000 + ((n - 0xd800) << 10) + (low - 0xdc00);
          } else if (n >= 0xdc00 && n <= 0xdfff)
            bad();
          utf(out, n);
          break;
        }
        default:
          bad();
        }
      } else if (c < 128)
        out += char(c);
      else {
        unsigned n = 0, count = 0, min = 0;
        if (c >= 0xc2 && c <= 0xdf) {
          n = c & 31;
          count = 1;
          min = 128;
        } else if (c >= 0xe0 && c <= 0xef) {
          n = c & 15;
          count = 2;
          min = 2048;
        } else if (c >= 0xf0 && c <= 0xf4) {
          n = c & 7;
          count = 3;
          min = 65536;
        } else
          bad();
        for (unsigned i = 0; i < count; ++i) {
          if (p == s.size())
            bad();
          unsigned char b = s[p++];
          if ((b & 0xc0) != 0x80)
            bad();
          n = (n << 6) | (b & 63);
        }
        if (n < min || n > 0x10ffff || (n >= 0xd800 && n <= 0xdfff))
          bad();
        utf(out, n);
      }
      if (out.size() > 4096)
        throw Problem("TraceLimitExceeded", "String exceeds path limit");
    }
    bad();
  }
  Json value(unsigned depth) {
    if (depth > 64)
      throw Problem("TraceLimitExceeded", "JSON nesting limit");
    ws();
    if (p == s.size())
      bad();
    char c = s[p];
    if (c == '"')
      return Json(string());
    if (c == '{' || c == '[') {
      ++p;
      Json j = c == '{' ? Json::obj() : Json::list();
      ws();
      char end = c == '{' ? '}' : ']';
      if (p < s.size() && s[p] == end) {
        ++p;
        return j;
      }
      for (;;) {
        ws();
        if (c == '{') {
          auto k = string();
          ws();
          if (p == s.size() || s[p++] != ':')
            bad();
          auto v = value(depth + 1);
          if (!j.object.emplace(k, std::move(v)).second)
            throw Problem("TraceInvalidSchema", "Duplicate key", {}, k);
        } else
          j.array.push_back(value(depth + 1));
        ws();
        if (p == s.size())
          bad();
        char sep = s[p++];
        if (sep == end)
          return j;
        if (sep != ',')
          bad();
      }
    }
    for (const auto &lit :
         {std::string("null"), std::string("true"), std::string("false")})
      if (s.compare(p, lit.size(), lit) == 0) {
        p += lit.size();
        return lit == "null" ? Json() : Json(lit == "true");
      }
    size_t start = p;
    if (c == '-')
      ++p;
    if (p == s.size())
      bad();
    if (s[p] == '0')
      ++p;
    else {
      if (s[p] < '1' || s[p] > '9')
        bad();
      while (p < s.size() && s[p] >= '0' && s[p] <= '9')
        ++p;
    }
    if (p < s.size() && s[p] == '.') {
      ++p;
      size_t q = p;
      while (p < s.size() && s[p] >= '0' && s[p] <= '9')
        ++p;
      if (p == q)
        bad();
    }
    if (p < s.size() && (s[p] == 'e' || s[p] == 'E')) {
      ++p;
      if (p < s.size() && (s[p] == '+' || s[p] == '-'))
        ++p;
      size_t q = p;
      while (p < s.size() && s[p] >= '0' && s[p] <= '9')
        ++p;
      if (p == q)
        bad();
    }
    Json j;
    j.type = Json::Number;
    j.value = s.substr(start, p - start);
    try {
      if (!std::isfinite(std::stod(j.value)))
        bad();
    } catch (const std::exception &) {
      bad();
    }
    return j;
  }

public:
  explicit Parser(const std::string &text) : s(text) {}
  Json parse() {
    auto j = value(0);
    ws();
    if (p != s.size())
      bad();
    return j;
  }
};
inline uint64_t uintValue(const Json &j, bool number = false) {
  if (j.type != (number ? Json::Number : Json::String))
    throw Problem("TraceInvalidSchema", "Expected unsigned integer");
  const auto &s = j.value;
  if (s.empty() || (s.size() > 1 && s[0] == '0'))
    throw Problem("TraceInvalidValue", "Noncanonical integer");
  uint64_t n = 0;
  for (char c : s) {
    if (c < '0' || c > '9' || n > (UINT64_MAX - uint64_t(c - '0')) / 10)
      throw Problem("TraceInvalidValue", "Integer out of range");
    n = n * 10 + uint64_t(c - '0');
  }
  return n;
}
inline int64_t intValue(const Json &j) {
  if (j.type != Json::String)
    throw Problem("TraceInvalidSchema", "Expected i64 string");
  auto s = j.value;
  bool neg = !s.empty() && s[0] == '-';
  if (neg)
    s.erase(0, 1);
  auto n = uintValue(Json(s));
  if ((neg && n == 0) || n > uint64_t(INT64_MAX) + (neg ? 1ULL : 0ULL))
    throw Problem("TraceInvalidValue", "i64 out of range");
  return neg ? (n == uint64_t(INT64_MAX) + 1 ? INT64_MIN : -int64_t(n))
             : int64_t(n);
}
inline Json failure(const Problem &p) {
  return Json::obj(
      {{"error", p.code},
       {"recordSequence", p.record.empty() ? Json() : Json(p.record)},
       {"field", p.field.empty() ? Json() : Json(p.field)},
       {"message", p.what()}});
}
} // namespace tracecli
