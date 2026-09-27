#include "utils/FileSystem.hpp"

#include <fstream>
#include <sstream>

#include "utils/Error.hpp"

namespace s2c::fs {

std::string absoluteOutputHint(const Path& dir) {
    // A single-component absolute path (/out) is almost always a typo for ./out.
    if (!dir.is_absolute()) return {};
    const Path rel = dir.relative_path();
    auto it = rel.begin();
    if (it == rel.end()) return {};
    const std::string name = it->string();
    if (name.empty() || name == "." || name == "..") return {};
    ++it;
    if (it != rel.end()) return {};
    return " Paths starting with '/' are absolute (the root of the disk). "
           "To write into a folder named \"" +
           name + "\" in the current directory, use --output ./" + name + ".";
}

void ensureDirectory(const Path& dir) {
    if (dir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec && !std::filesystem::is_directory(dir)) {
        throw ConversionError("Could not create directory \"" + dir.string() + "\": " + ec.message() + "." +
                              absoluteOutputHint(dir));
    }
}

void requireWritableDirectory(const Path& dir, const std::string& what) {
    std::error_code ec;
    Path abs = std::filesystem::weakly_canonical(dir, ec);
    if (ec || abs.empty()) abs = std::filesystem::absolute(dir, ec);
    if (ec || abs.empty()) abs = dir;

    if (std::filesystem::exists(abs, ec)) {
        if (!std::filesystem::is_directory(abs)) {
            throw ConversionError(what + " \"" + dir.string() + "\" exists and is not a directory.");
        }
    } else {
        std::filesystem::create_directories(abs, ec);
        if (ec) {
            throw ConversionError("Could not create " + what + " \"" + dir.string() + "\": " + ec.message() + "." +
                                  absoluteOutputHint(dir));
        }
    }

    const Path probe = abs / ".scratch2cpp-write-test";
    {
        std::ofstream out(probe, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw ConversionError(what + " \"" + dir.string() + "\" is not writable." + absoluteOutputHint(dir));
        }
        out << 'x';
        if (!out) {
            std::filesystem::remove(probe, ec);
            throw ConversionError(what + " \"" + dir.string() + "\" is not writable." + absoluteOutputHint(dir));
        }
    }
    std::filesystem::remove(probe, ec);
}

void writeTextFile(const Path& path, const std::string& contents) {
    ensureDirectory(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw ConversionError("Could not write file \"" + path.string() + "\"");
    out << contents;
    if (!out) throw ConversionError("Failed while writing \"" + path.string() + "\"");
}

void writeBinaryFile(const Path& path, const std::vector<std::uint8_t>& bytes) {
    ensureDirectory(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw ConversionError("Could not write file \"" + path.string() + "\"");
    if (!bytes.empty()) out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw ConversionError("Failed while writing \"" + path.string() + "\"");
}

std::vector<std::uint8_t> readBinaryFile(const Path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw ConversionError("Could not read file \"" + path.string() + "\"");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return bytes;
}

std::string readTextFile(const Path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw ConversionError("Could not read file \"" + path.string() + "\"");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void makeExecutable(const Path& path) {
#ifndef _WIN32
    std::error_code ec;
    std::filesystem::permissions(path,
                                 std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add, ec);
#else
    (void)path;
#endif
}

}  // namespace s2c::fs
