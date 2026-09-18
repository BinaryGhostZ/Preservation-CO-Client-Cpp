#pragma once

#include "common.hpp"
#include <cstdint>
#include <unordered_map>
#include <span>
#include <optional>

namespace sandbox_data {

constexpr uint32_t WDF_HEADER_SIGNATURE = 0x57444650; // "WDFP"
constexpr size_t WDF_DIRECTORY_ENTRY_LENGTH = 16;

struct WdfHeader {
    uint32_t entry_count;
    uint32_t directory_offset;

    static WdfHeader parse(std::span<const uint8_t> bytes);
    size_t directory_length() const;
};

struct WdfEntry {
    uint32_t id;
    uint32_t offset;
    uint32_t size;
    uint32_t reserved;
};

class WdfDirectory {
    std::unordered_map<uint32_t, WdfEntry> entries_;

public:
    WdfDirectory() = default;

    static WdfDirectory parse(const WdfHeader& header, std::span<const uint8_t> directory_bytes, uint64_t archive_length);

    std::optional<WdfEntry> get(uint32_t id) const;
    bool contains_id(uint32_t id) const;
    size_t len() const;
    bool is_empty() const;
};

} // namespace sandbox_data
