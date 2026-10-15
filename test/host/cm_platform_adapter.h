// [MOCKED!] Host adapters for Arduino String and ESP32 Preferences only.
// Config construction, key reservation, registration, loading and JSON use real CM source.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <cstdarg>

class AsyncWebServer; // [MOCKED!] Only a forward declaration; no server is constructed.
class String {
    std::string text_;
public:
    String() = default;
    String(const char* value) : text_(value ? value : "") {}
    String(const std::string& value) : text_(value) {}
    template<class T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    String(T value) : text_(std::to_string(value)) {}
    const char* c_str() const { return text_.c_str(); }
    size_t length() const { return text_.length(); }
    bool isEmpty() const { return text_.empty(); }
    char operator[](size_t index) const { return text_[index]; }
    void toLowerCase() { std::transform(text_.begin(), text_.end(), text_.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); }
    void replace(const char* from, const char* to) {
        const std::string needle(from), replacement(to);
        for(size_t pos = 0; (pos = text_.find(needle, pos)) != std::string::npos; pos += replacement.size()) text_.replace(pos, needle.size(), replacement);
    }
    String substring(size_t start, size_t end) const { return text_.substr(start, end - start); }
    String& operator+=(const String& other) { text_ += other.text_; return *this; }
    friend String operator+(const String& left, const String& right) { return left.text_ + right.text_; }
    friend bool operator==(const String& left, const String& right) { return left.text_ == right.text_; }
    friend bool operator!=(const String& left, const String& right) { return !(left == right); }
    friend bool operator<(const String& left, const String& right) { return left.text_ < right.text_; }
};

class Preferences {
    using Value = std::variant<String, bool, int, float>;
    std::map<std::string, Value> values_;
    template<class T> T read(const char* key, const T& fallback) const {
        const auto found = values_.find(key);
        if(found == values_.end()) return fallback;
        const T* value = std::get_if<T>(&found->second);
        return value ? *value : fallback;
    }
    template<class T> size_t write(const char* key, const T& value, size_t bytes) {
        ++writeAttempts;
        if(failWrites) return 0;
        if(reportSuccessWithoutWrite) return bytes;
        values_.insert_or_assign(key, value);
        return bytes;
    }
public:
    bool failWrites = false;
    bool reportSuccessWithoutWrite = false;
    size_t writeAttempts = 0;
    bool isKey(const char* key) const { return values_.count(key) != 0; }
    String getString(const char* key, String fallback = String()) const { return read(key, fallback); }
    bool getBool(const char* key, bool fallback = false) const { return read(key, fallback); }
    int getInt(const char* key, int fallback = 0) const { return read(key, fallback); }
    float getFloat(const char* key, float fallback = 0) const { return read(key, fallback); }
    // ESP32 Preferences returns strlen(value): a successful empty write returns zero.
    size_t putString(const char* key, const String& value) { return write(key, value, value.length()); }
    size_t putBool(const char* key, bool value) { return write(key, value, sizeof(value)); }
    size_t putInt(const char* key, int value) { return write(key, value, sizeof(value)); }
    size_t putFloat(const char* key, float value) { return write(key, value, sizeof(value)); }
};

#include <ArduinoJson.h>
namespace ArduinoJson {
template<> struct Converter<String> {
    static void toJson(const String& source, JsonVariant destination) { destination.set(source.c_str()); }
    static String fromJson(JsonVariantConst source) { return String(source.as<const char*>()); }
    static bool checkJson(JsonVariantConst source) { return source.is<const char*>(); }
};
}