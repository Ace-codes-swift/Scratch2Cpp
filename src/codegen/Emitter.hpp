// Emitter.hpp - indentation-aware text builder for generated C++.
#pragma once

#include <string>

namespace s2c::codegen {

class Emitter {
public:
    void line(const std::string& text = "") {
        if (text.empty()) {
            out_ += '\n';
            return;
        }
        out_.append(static_cast<size_t>(indent_) * 4, ' ');
        out_ += text;
        out_ += '\n';
    }
    // Opens a block: emits "text {" and indents.
    void open(const std::string& text) {
        line(text.empty() ? "{" : text + " {");
        ++indent_;
    }
    // Closes a block: dedents and emits "}" (plus optional suffix, e.g. ";").
    void close(const std::string& suffix = "") {
        if (indent_ > 0) --indent_;
        line("}" + suffix);
    }
    void indent() { ++indent_; }
    void dedent() { if (indent_ > 0) --indent_; }
    void raw(const std::string& text) { out_ += text; }
    int depth() const { return indent_; }
    const std::string& str() const { return out_; }

private:
    std::string out_;
    int indent_ = 0;
};

}  // namespace s2c::codegen
