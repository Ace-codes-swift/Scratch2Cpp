// Value.hpp - Scratch's dynamically typed value.
//
// Scratch values are JavaScript values in scratch-vm (number, string or
// boolean) and every block casts its inputs with the rules in
// scratch-vm/src/util/cast.js. This class reproduces those casting rules so
// generated code behaves like the original project.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace scratch {

class Value {
public:
    enum class Type : std::uint8_t { Number, String, Bool };

    Value() : type_(Type::String) {}
    Value(double n) : type_(Type::Number), number_(n) {}
    Value(int n) : Value(static_cast<double>(n)) {}
    Value(long n) : Value(static_cast<double>(n)) {}
    Value(long long n) : Value(static_cast<double>(n)) {}
    Value(unsigned long n) : Value(static_cast<double>(n)) {}
    Value(unsigned long long n) : Value(static_cast<double>(n)) {}
    Value(bool b) : type_(Type::Bool), boolean_(b) {}
    Value(std::string s) : type_(Type::String), string_(std::move(s)) {}
    Value(const char* s) : type_(Type::String), string_(s ? s : "") {}
    Value(std::string_view s) : type_(Type::String), string_(s) {}

    Type type() const { return type_; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isBool() const { return type_ == Type::Bool; }

    // Casting rules from scratch-vm Cast.toNumber / toString / toBoolean.
    double toNumber() const;
    std::string toString() const;
    bool toBool() const;

    // True when the value is a whole number (Cast.isInt).
    bool isInt() const;
    // True when the value would be treated as whitespace by Cast.compare.
    bool isWhitespace() const;

    // Cast.compare: <0, 0, >0. Compares numerically when both sides are
    // numeric, otherwise case-insensitively as strings.
    static double compare(const Value& a, const Value& b);

    // JavaScript Number(...) semantics for strings, returning NaN on failure.
    static double parseNumber(std::string_view text);
    // JavaScript String(number) semantics (shortest round-trip, "Infinity"...).
    static std::string formatNumber(double n);

private:
    Type type_;
    double number_ = 0.0;
    bool boolean_ = false;
    std::string string_;
};

}  // namespace scratch
