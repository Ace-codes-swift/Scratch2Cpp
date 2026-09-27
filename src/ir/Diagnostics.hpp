// Diagnostics.hpp - collects warnings produced during parsing/generation.
//
// Unsupported blocks must never disappear silently: every one is recorded
// here, printed to the console and listed in the generated README.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace s2c {

struct Diagnostic {
    enum class Kind { Warning, UnsupportedBlock, UnsupportedFeature };
    Kind kind;
    std::string message;
    std::string target;    // sprite/stage name when applicable
    std::string opcode;    // for UnsupportedBlock
};

class Diagnostics {
public:
    void warning(const std::string& message, const std::string& target = "");
    void unsupportedBlock(const std::string& opcode, const std::string& target, const std::string& detail = "");
    void unsupportedFeature(const std::string& feature, const std::string& target = "");

    const std::vector<Diagnostic>& all() const { return items_; }
    bool empty() const { return items_.empty(); }
    size_t count() const { return items_.size(); }

    // opcode -> number of occurrences, for summaries.
    std::map<std::string, int> unsupportedOpcodeCounts() const;

private:
    std::vector<Diagnostic> items_;
};

}  // namespace s2c
