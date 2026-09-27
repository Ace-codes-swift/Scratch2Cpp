// List.hpp - Scratch list with 1-based indexing and Scratch's index rules.
#pragma once

#include <string>
#include <vector>

#include "scratch/Value.hpp"

namespace scratch {

class List {
public:
    List() = default;
    explicit List(std::vector<Value> items) : items_(std::move(items)) {}

    // Index resolution as in scratch-vm Cast.toListIndex: numbers are
    // rounded, "last"/"random"/"any" are special, everything else is invalid.
    // Returns 0 for an invalid index, otherwise a 1-based index.
    std::size_t resolveIndex(const Value& index, bool allowLengthPlusOne = false) const;

    void add(const Value& v) { items_.push_back(v); }
    void deleteAt(const Value& index);
    void clear() { items_.clear(); }
    void insertAt(const Value& index, const Value& v);
    void replaceAt(const Value& index, const Value& v);
    Value itemAt(const Value& index) const;
    // 1-based position of `item`, or 0 if absent (Cast.compare equality).
    double indexOf(const Value& item) const;
    bool contains(const Value& item) const { return indexOf(item) != 0; }
    std::size_t length() const { return items_.size(); }
    // Reporter value: items joined with spaces, or concatenated when every
    // item is a single character (scratch-vm behaviour).
    std::string contents() const;

    std::vector<Value>& items() { return items_; }
    const std::vector<Value>& items() const { return items_; }

private:
    std::vector<Value> items_;
};

}  // namespace scratch
