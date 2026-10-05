// Minimal JSON reader for the CPU test data (HANDOFF 6.12): the V20 tests
// and both metadata.json files. Enough of RFC 8259 for that input:
// objects, arrays, integers, strings without \u escapes beyond ASCII,
// true/false/null. Not a general-purpose parser.

#ifndef ARIA_TESTS_JSON_LITE_H
#define ARIA_TESTS_JSON_LITE_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct JsonValue {
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject } type = Type::kNull;
  int64_t number = 0;
  bool boolean = false;
  std::string text;
  std::vector<JsonValue> items;
  std::map<std::string, JsonValue> members;

  const JsonValue* Find(const std::string& key) const {
    const auto it = members.find(key);
    return it == members.end() ? nullptr : &it->second;
  }
};

class JsonReader {
 public:
  JsonReader(const char* data, std::size_t length) : data_(data), end_(data + length) {}

  bool Parse(JsonValue& value) {
    ok_ = true;
    ParseValue(value);
    Skip();
    return ok_;
  }

 private:
  void Skip() {
    while (data_ < end_ && (*data_ == ' ' || *data_ == '\n' || *data_ == '\r' || *data_ == '\t')) ++data_;
  }

  bool Expect(char ch) {
    Skip();
    if (data_ < end_ && *data_ == ch) {
      ++data_;
      return true;
    }
    ok_ = false;
    return false;
  }

  void ParseString(std::string& out) {
    out.clear();
    if (!Expect('"')) return;
    while (data_ < end_ && *data_ != '"') {
      if (*data_ == '\\' && data_ + 1 < end_) {
        ++data_;
        const char escaped = *data_;
        out.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
        if (escaped == 'u') data_ += 4;  // Not needed by the test data.
      } else {
        out.push_back(*data_);
      }
      ++data_;
    }
    if (data_ >= end_) {
      ok_ = false;
      return;
    }
    ++data_;
  }

  void ParseValue(JsonValue& value) {
    Skip();
    if (data_ >= end_ || !ok_) {
      ok_ = false;
      return;
    }
    const char ch = *data_;
    if (ch == '{') {
      value.type = JsonValue::Type::kObject;
      ++data_;
      Skip();
      if (data_ < end_ && *data_ == '}') {
        ++data_;
        return;
      }
      while (ok_) {
        std::string key;
        Skip();
        ParseString(key);
        if (!Expect(':')) return;
        ParseValue(value.members[key]);
        Skip();
        if (data_ < end_ && *data_ == ',') {
          ++data_;
          continue;
        }
        Expect('}');
        return;
      }
    } else if (ch == '[') {
      value.type = JsonValue::Type::kArray;
      ++data_;
      Skip();
      if (data_ < end_ && *data_ == ']') {
        ++data_;
        return;
      }
      while (ok_) {
        value.items.emplace_back();
        ParseValue(value.items.back());
        Skip();
        if (data_ < end_ && *data_ == ',') {
          ++data_;
          continue;
        }
        Expect(']');
        return;
      }
    } else if (ch == '"') {
      value.type = JsonValue::Type::kString;
      ParseString(value.text);
    } else if (ch == 't' || ch == 'f' || ch == 'n') {
      const std::string word = ch == 't' ? "true" : ch == 'f' ? "false" : "null";
      if (static_cast<std::size_t>(end_ - data_) < word.size() || std::string(data_, word.size()) != word) {
        ok_ = false;
        return;
      }
      data_ += word.size();
      value.type = ch == 'n' ? JsonValue::Type::kNull : JsonValue::Type::kBool;
      value.boolean = ch == 't';
    } else {
      // Integers only: the test data has no fractions or exponents.
      value.type = JsonValue::Type::kNumber;
      bool negative = false;
      if (*data_ == '-') {
        negative = true;
        ++data_;
      }
      if (data_ >= end_ || *data_ < '0' || *data_ > '9') {
        ok_ = false;
        return;
      }
      int64_t number = 0;
      while (data_ < end_ && *data_ >= '0' && *data_ <= '9') number = number * 10 + (*data_++ - '0');
      value.number = negative ? -number : number;
    }
  }

  const char* data_;
  const char* end_;
  bool ok_ = true;
};

#endif
