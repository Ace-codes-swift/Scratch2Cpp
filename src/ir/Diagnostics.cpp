#include "ir/Diagnostics.hpp"

#include "utils/Log.hpp"

namespace s2c {

void Diagnostics::warning(const std::string& message, const std::string& target) {
    items_.push_back({Diagnostic::Kind::Warning, message, target, ""});
    log::warning(target.empty() ? message : message + " in sprite \"" + target + "\"");
}

void Diagnostics::unsupportedBlock(const std::string& opcode, const std::string& target, const std::string& detail) {
    std::string message = "Unsupported Scratch block \"" + opcode + "\" in sprite \"" + target + "\"";
    if (!detail.empty()) message += " (" + detail + ")";
    items_.push_back({Diagnostic::Kind::UnsupportedBlock, message, target, opcode});
    log::warning(message);
}

void Diagnostics::unsupportedFeature(const std::string& feature, const std::string& target) {
    std::string message = "Unsupported Scratch feature: " + feature;
    if (!target.empty()) message += " in sprite \"" + target + "\"";
    items_.push_back({Diagnostic::Kind::UnsupportedFeature, message, target, ""});
    log::warning(message);
}

std::map<std::string, int> Diagnostics::unsupportedOpcodeCounts() const {
    std::map<std::string, int> counts;
    for (const auto& d : items_) {
        if (d.kind == Diagnostic::Kind::UnsupportedBlock) ++counts[d.opcode];
    }
    return counts;
}

}  // namespace s2c
