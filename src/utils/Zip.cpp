#include "utils/Zip.hpp"

#include <miniz.h>

#include "utils/Error.hpp"

namespace s2c {

struct ZipArchive::Impl {
    std::vector<std::uint8_t> bytes;
    mz_zip_archive zip{};
    bool open = false;
};

ZipArchive::ZipArchive(std::vector<std::uint8_t> bytes) : impl_(new Impl) {
    impl_->bytes = std::move(bytes);
    mz_zip_zero_struct(&impl_->zip);
    if (!mz_zip_reader_init_mem(&impl_->zip, impl_->bytes.data(), impl_->bytes.size(), 0)) {
        const std::string err = mz_zip_get_error_string(mz_zip_get_last_error(&impl_->zip));
        delete impl_;
        throw ConversionError("Not a valid .sb3/ZIP archive: " + err);
    }
    impl_->open = true;
}

ZipArchive::~ZipArchive() {
    if (impl_) {
        if (impl_->open) mz_zip_reader_end(&impl_->zip);
        delete impl_;
    }
}

std::vector<std::string> ZipArchive::entryNames() const {
    std::vector<std::string> names;
    const mz_uint count = mz_zip_reader_get_num_files(&impl_->zip);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (mz_zip_reader_file_stat(&impl_->zip, i, &st) && !st.m_is_directory) {
            names.emplace_back(st.m_filename);
        }
    }
    return names;
}

bool ZipArchive::hasEntry(const std::string& name) const {
    return mz_zip_reader_locate_file(&impl_->zip, name.c_str(), nullptr, 0) >= 0;
}

std::vector<std::uint8_t> ZipArchive::read(const std::string& name) const {
    const int index = mz_zip_reader_locate_file(&impl_->zip, name.c_str(), nullptr, 0);
    if (index < 0) throw ConversionError("Archive entry not found: " + name);
    size_t size = 0;
    void* data = mz_zip_reader_extract_to_heap(&impl_->zip, static_cast<mz_uint>(index), &size, 0);
    if (!data) throw ConversionError("Could not extract archive entry: " + name);
    std::vector<std::uint8_t> out(static_cast<std::uint8_t*>(data), static_cast<std::uint8_t*>(data) + size);
    mz_free(data);
    return out;
}

}  // namespace s2c
