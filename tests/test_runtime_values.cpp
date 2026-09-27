// Tests for Scratch value casting / operator semantics in the runtime.
#include <cmath>
#include <iostream>
#include <string>

#include "scratch/List.hpp"
#include "scratch/Ops.hpp"
#include "scratch/Value.hpp"

using scratch::Value;

namespace {
int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

template <class T>
void checkEq(const T& actual, const T& expected, const std::string& what) {
    if (!(actual == expected)) {
        std::cerr << "FAIL: " << what << " -> got '" << actual << "', expected '" << expected << "'\n";
        ++failures;
    }
}
}  // namespace

int main() {
    // Number formatting follows JavaScript's Number#toString.
    checkEq(Value(1.0).toString(), std::string("1"), "1.0 -> \"1\"");
    checkEq(Value(0.1 + 0.2).toString(), std::string("0.30000000000000004"), "0.1+0.2");
    checkEq(Value(-0.0).toString(), std::string("0"), "-0");
    checkEq(Value(1e21).toString(), std::string("1e+21"), "1e21");
    checkEq(Value(1e-7).toString(), std::string("1e-7"), "1e-7");
    checkEq(Value(123456789012.0).toString(), std::string("123456789012"), "large int");
    checkEq(Value(0.5).toString(), std::string("0.5"), "0.5");
    checkEq(Value(1.0 / 0.0).toString(), std::string("Infinity"), "Infinity");

    // String -> number follows JavaScript Number().
    checkEq(Value("  42 ").toNumber(), 42.0, "trimmed number");
    checkEq(Value("").toNumber(), 0.0, "empty -> 0");
    checkEq(Value("abc").toNumber(), 0.0, "NaN -> 0");
    checkEq(Value("0x10").toNumber(), 16.0, "hex");
    checkEq(Value("1e3").toNumber(), 1000.0, "exponent");
    checkEq(Value("Infinity").toNumber(), std::numeric_limits<double>::infinity(), "Infinity string");
    checkEq(Value(true).toNumber(), 1.0, "true -> 1");

    // Booleans.
    check(!Value("false").toBool(), "\"false\" is false");
    check(!Value("FALSE").toBool(), "\"FALSE\" is false");
    check(!Value("0").toBool(), "\"0\" is false");
    check(!Value("").toBool(), "\"\" is false");
    check(Value("hello").toBool(), "\"hello\" is true");
    check(Value(2.0).toBool(), "2 is true");
    check(!Value(0.0).toBool(), "0 is false");

    // Comparison.
    check(scratch::ops::eq(Value("10"), Value(10.0)), "\"10\" == 10");
    check(scratch::ops::eq(Value("abc"), Value("ABC")), "case-insensitive equality");
    check(scratch::ops::lt(Value("apple"), Value("banana")), "string ordering");
    check(scratch::ops::gt(Value(10.0), Value("9")), "numeric ordering with strings");
    check(!scratch::ops::eq(Value(" "), Value(0.0)), "whitespace is not 0");
    check(scratch::ops::eq(Value(""), Value("")), "empty == empty");
    check(scratch::ops::lt(Value(""), Value("a")), "empty < a");

    // Arithmetic.
    checkEq(scratch::ops::mod(-7.0, 3.0), 2.0, "mod sign follows divisor");
    checkEq(scratch::ops::mod(7.0, -3.0), -2.0, "mod negative divisor");
    checkEq(scratch::ops::round(2.5), 3.0, "round half up");
    checkEq(scratch::ops::round(-2.5), -2.0, "round negative half toward +inf");
    check(std::isinf(scratch::ops::div(1.0, 0.0)), "1/0 = Infinity");
    check(std::isnan(scratch::ops::div(0.0, 0.0)), "0/0 = NaN");
    checkEq(scratch::ops::mathop("sin", 90.0), 1.0, "sin 90");
    checkEq(scratch::ops::mathop("cos", 90.0), 0.0, "cos 90 exactly 0");
    checkEq(scratch::ops::mathop("floor", -1.5), -2.0, "floor");
    for (int i = 0; i < 100; ++i) {
        const double r = scratch::ops::random(Value(1.0), Value(3.0));
        check(r >= 1 && r <= 3 && std::floor(r) == r, "random int in range");
    }
    check(scratch::ops::random(Value(0.5), Value(0.5)) == 0.5, "random same bounds");

    // Strings.
    checkEq(scratch::ops::join(Value("a"), Value(1.5)), std::string("a1.5"), "join");
    checkEq(scratch::ops::letterOf(Value(1.0), Value("héllo")), std::string("h"), "letter 1");
    checkEq(scratch::ops::letterOf(Value(2.0), Value("héllo")), std::string("é"), "letter 2 (utf-8)");
    checkEq(scratch::ops::letterOf(Value(9.0), Value("hi")), std::string(""), "letter out of range");
    checkEq(scratch::ops::length(Value("héllo")), 5.0, "length utf-8");
    check(scratch::ops::contains(Value("Hello World"), Value("world")), "contains case-insensitive");

    // Lists.
    scratch::List list;
    list.add(Value("a"));
    list.add(Value("b"));
    list.add(Value("c"));
    checkEq(list.itemAt(Value("last")).toString(), std::string("c"), "item last");
    checkEq(list.itemAt(Value(0.0)).toString(), std::string(""), "item 0 is empty");
    checkEq(list.indexOf(Value("B")), 2.0, "index of (case-insensitive)");
    list.insertAt(Value(4.0), Value("d"));
    checkEq(list.length(), static_cast<std::size_t>(4), "insert at length+1");
    list.deleteAt(Value(1.0));
    checkEq(list.contents(), std::string("bcd"), "single-char contents concatenated");
    list.replaceAt(Value(1.0), Value("bee"));
    checkEq(list.contents(), std::string("bee c d"), "contents with spaces");
    list.deleteAt(Value("all"));
    checkEq(list.length(), static_cast<std::size_t>(0), "delete all");

    if (failures == 0) std::cout << "test_runtime_values: all checks passed\n";
    return failures == 0 ? 0 : 1;
}
