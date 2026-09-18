#include "wdf.hpp"
#include "reader.hpp"
#include <limits>

namespace sandbox_data {

WdfHeader WdfHeader::parse(std::span<const uint8_t> bytes) {
    SliceReader reader("WDF header", bytes);
    uint32_t signature = reader.read_u32_le();
    
    if (signature != WDF_HEADER_SIGNATURE) {
        throw ContentError("WDF header", "unexpected signature");
    }

    WdfHeader header;
    header.entry_count = reader.read_u32_le();
    header.directory_offset = reader.read_u32_le();
    return header;
}

size_t WdfHeader::directory_length() const {
    if (entry_count > std::numeric_limits<size_t>::max() / WDF_DIRECTORY_ENTRY_LENGTH) {
        throw ContentError("WDF header", "directory length overflows this platform");
    }
    return static_cast<size_t>(entry_count) * WDF_DIRECTORY_ENTRY_LENGTH;
}

WdfDirectory WdfDirectory::parse(const WdfHeader& header, std::span<const uint8_t> directory_bytes, uint64_t archive_length) {
    size_t expected_length = header.directory_length();
    if (directory_bytes.size() != expected_length) {
        throw ContentError("WDF directory", 
            "expected " + std::to_string(expected_length) + " bytes for entries");
    }

    SliceReader reader("WDF directory", directory_bytes);
    WdfDirectory dir;
    dir.entries_.reserve(header.entry_count);

    for (uint32_t i = 0; i < header.entry_count; ++i) {
        WdfEntry entry;
        entry.id = reader.read_u32_le();
        entry.offset = reader.read_u32_le();
        entry.size = reader.read_u32_le();
        entry.reserved = reader.read_u32_le();

        uint64_t end = static_cast<uint64_t>(entry.offset) + static_cast<uint64_t>(entry.size);
        if (end > archive_length) {
            throw ContentError("WDF directory", 
                "entry " + std::to_string(entry.id) + " ends beyond archive length");
        }

        if (dir.entries_.find(entry.id) != dir.entries_.end()) {
            throw ContentError("WDF directory", 
                "entry " + std::to_string(entry.id) + " appears more than once");
        }

        dir.entries_.emplace(entry.id, entry);
    }

    return dir;
}

std::optional<WdfEntry> WdfDirectory::get(uint32_t id) const {
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool WdfDirectory::contains_id(uint32_t id) const {
    return entries_.find(id) != entries_.end();
}

size_t WdfDirectory::len() const {
    return entries_.size();
}

bool WdfDirectory::is_empty() const {
    return entries_.empty();
}

} // namespace sandbox_data
