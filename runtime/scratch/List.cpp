#include "scratch/List.hpp"

#include <cmath>
#include <random>

namespace scratch {

namespace {
std::mt19937& rng() {
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}
}  // namespace

std::size_t List::resolveIndex(const Value& index, bool allowLengthPlusOne) const {
    const std::size_t length = items_.size() + (allowLengthPlusOne ? 1 : 0);
    if (index.isString()) {
        const std::string s = index.toString();
        if (s == "last") return length > 0 ? length : 0;
        if (s == "random" || s == "any") {
            if (length == 0) return 0;
            std::uniform_int_distribution<std::size_t> dist(1, length);
            return dist(rng());
        }
    }
    const double n = std::floor(index.toNumber());
    if (n < 1 || n > static_cast<double>(length)) return 0;
    return static_cast<std::size_t>(n);
}

void List::deleteAt(const Value& index) {
    if (index.isString() && index.toString() == "all") {
        items_.clear();
        return;
    }
    const std::size_t i = resolveIndex(index);
    if (i == 0) return;
    items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i - 1));
}

void List::insertAt(const Value& index, const Value& v) {
    const std::size_t i = resolveIndex(index, /*allowLengthPlusOne=*/true);
    if (i == 0) return;
    items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(i - 1), v);
}

void List::replaceAt(const Value& index, const Value& v) {
    const std::size_t i = resolveIndex(index);
    if (i == 0) return;
    items_[i - 1] = v;
}

Value List::itemAt(const Value& index) const {
    const std::size_t i = resolveIndex(index);
    if (i == 0) return Value("");
    return items_[i - 1];
}

double List::indexOf(const Value& item) const {
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (Value::compare(items_[i], item) == 0) return static_cast<double>(i + 1);
    }
    return 0;
}

std::string List::contents() const {
    bool allSingleChars = true;
    for (const Value& v : items_) {
        if (v.toString().size() != 1) { allSingleChars = false; break; }
    }
    std::string out;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (i > 0 && !allSingleChars) out += ' ';
        out += items_[i].toString();
    }
    return out;
}

}  // namespace scratch
