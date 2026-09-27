// ScratchParser.hpp - turns a Scratch 3 project.json into the IR.
//
// This is the "Scratch frontend". It understands the sb3 serialisation format
// (block graph with next/parent links, input shadow encoding, primitive
// arrays, mutations) and produces the source-agnostic ir::Project.
#pragma once

#include "ir/Diagnostics.hpp"
#include "ir/Project.hpp"
#include "scratch/ProjectSource.hpp"

namespace s2c {

class ScratchParser {
public:
    explicit ScratchParser(Diagnostics& diagnostics) : diagnostics_(diagnostics) {}

    // Throws ConversionError for malformed projects.
    ir::Project parse(const RawProject& raw);

private:
    Diagnostics& diagnostics_;
};

}  // namespace s2c
