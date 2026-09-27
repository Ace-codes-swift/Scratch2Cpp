// Error.hpp - exception type for user-facing conversion failures.
#pragma once

#include <stdexcept>
#include <string>

namespace s2c {

// Thrown for errors that should be reported to the user with a clear,
// actionable message (bad URL, download failure, malformed project...).
class ConversionError : public std::runtime_error {
public:
    explicit ConversionError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace s2c
