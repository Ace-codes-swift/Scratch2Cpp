// Expr.hpp - a generated C++ expression together with its static type.
//
// Scratch is dynamically typed, but most blocks have statically known result
// types (operators return numbers, comparisons return booleans...). Tracking
// the type lets the generator emit plain doubles/bools where possible and
// only fall back to scratch::Value (with Scratch casting rules) when needed.
#pragma once

#include <string>

namespace s2c::codegen {

enum class CppType { Number, String, Bool, Value };

struct Expr {
    std::string code;
    CppType type = CppType::Value;

    static Expr number(std::string code) { return {std::move(code), CppType::Number}; }
    static Expr string(std::string code) { return {std::move(code), CppType::String}; }
    static Expr boolean(std::string code) { return {std::move(code), CppType::Bool}; }
    static Expr value(std::string code) { return {std::move(code), CppType::Value}; }

    // Conversions apply scratch::Value casting semantics.
    std::string asNumber() const {
        switch (type) {
        case CppType::Number: return code;
        case CppType::Value: return "(" + code + ").toNumber()";
        case CppType::Bool: return "((" + code + ") ? 1.0 : 0.0)";
        case CppType::String: return "scratch::Value(" + code + ").toNumber()";
        }
        return code;
    }
    std::string asString() const {
        switch (type) {
        case CppType::String: return code;
        case CppType::Value: return "(" + code + ").toString()";
        case CppType::Bool: return "std::string((" + code + ") ? \"true\" : \"false\")";
        case CppType::Number: return "scratch::Value(" + code + ").toString()";
        }
        return code;
    }
    std::string asBool() const {
        switch (type) {
        case CppType::Bool: return code;
        case CppType::Value: return "(" + code + ").toBool()";
        case CppType::Number: return "scratch::Value(" + code + ").toBool()";
        case CppType::String: return "scratch::Value(" + code + ").toBool()";
        }
        return code;
    }
    std::string asValue() const {
        if (type == CppType::Value) return code;
        return "scratch::Value(" + code + ")";
    }
};

}  // namespace s2c::codegen
