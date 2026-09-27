// Ops.hpp - Scratch operator blocks (the "Operators" palette).
//
// Generated code calls these helpers instead of using raw C++ operators so
// that JavaScript/Scratch semantics (NaN handling, string comparisons,
// 1-based letters, ...) are preserved.
#pragma once

#include <string>

#include "scratch/Value.hpp"

namespace scratch::ops {

double add(double a, double b);
double sub(double a, double b);
double mul(double a, double b);
double div(double a, double b);      // JS division: x/0 -> Infinity, 0/0 -> NaN
double mod(double a, double b);      // Scratch mod: result takes the sign of the divisor
double random(const Value& from, const Value& to);  // integer result when both bounds are integers
double round(double a);              // JS Math.round (rounds .5 up)
double mathop(const std::string& op, double a);

bool lt(const Value& a, const Value& b);
bool gt(const Value& a, const Value& b);
bool eq(const Value& a, const Value& b);

std::string join(const Value& a, const Value& b);
std::string letterOf(const Value& index, const Value& text);  // 1-based, "" when out of range
double length(const Value& text);
bool contains(const Value& text, const Value& needle);        // case-insensitive

}  // namespace scratch::ops
