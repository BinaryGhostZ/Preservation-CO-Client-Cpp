#include "map_puzzle.hpp"
#include "reader.hpp"
#include <unordered_set>
#include <algorithm>

namespace sandbox_data {

constexpr uint32_t TERRAIN_TOKEN = 1000;
constexpr uint32_t COVERAGE_MASK = 0x01ffffff;

bool is_supported_5065_pux_material(const std::array<uint32_t, 5>& parameters) {
    return parameters[0] == 5 && parameters[1] == 6 && parameters[2] == 4;
}

static std::string nul_terminated_ascii(std::span<const uint8_t> bytes, const char* owner) {
    size_t end = 0;
    for (; end < bytes.size(); ++end) {
        if (bytes[end] == '\0') break;
    }
    std::string str(reinterpret_cast<const char*>(bytes.data()), end);
    std::replace(str.begin(), str.end(), '\\', '/');
    return str;
}

static std::string sized_ascii(SliceReader& reader, const char* owner) {
    size_t length = reader.read_u16_le();
    return nul_terminated_ascii(reader.read_exact(length), owner);
}

static void expect_token(SliceReader& reader, const char* owner) {
    uint32_t token = reader.read_u32_le();
    if (token != TERRAIN_TOKEN) {
        throw ContentError("PUX", std::string(owner) + " token mismatch");
    }
}

static std::optional<int16_t> puzzle_section_id(const std::string& section) {
    if (section.find("Puzzle") == 0) {
        try {
            return static_cast<int16_t>(std::stoi(section.substr(6)));
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

static std::vector<PuzzleTextureGroup> read_groups(SliceReader& reader, const char* owner) {
    size_t count = reader.read_u16_le();
    std::vector<PuzzleTextureGroup> groups;
    groups.reserve(count);
    std::unordered_set<int16_t> ids;

    for (size_t i = 0; i < count; ++i) {
        size_t label_length = reader.read_u16_le();
        auto label_span = reader.read_exact(label_length);
        std::vector<uint8_t> label(label_span.begin(), label_span.end());

        std::string ani = sized_ascii(reader, owner);
        std::string section = sized_ascii(reader, owner);

        auto opt_id = puzzle_section_id(section);
        if (!opt_id) {
            throw ContentError("PUX", "invalid puzzle section ID");
        }
        int16_t tile_id = *opt_id;

        if (ids.find(tile_id) != ids.end()) {
            throw ContentError("PUX", "tile ID declared more than once");
        }
        ids.insert(tile_id);

        PuzzleTextureGroup group;
        group.tile_id = tile_id;
        group.label = std::move(label);
        group.ani_path = ani;
        group.section = section;
        for (int j = 0; j < 5; ++j) {
            group.parameters[j] = reader.read_u32_le();
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

static PuzzleLayout parse_pux(std::span<const uint8_t> bytes) {
    SliceReader reader("PUX", bytes);
    std::string kind = nul_terminated_ascii(reader.read_exact(16), "PUX signature");
    if (kind != "TqTerrain") {
        throw ContentError("PUX", "unexpected PUX kind");
    }

    expect_token(reader, "PUX header");
    uint32_t columns = reader.read_u32_le();
    uint32_t rows = reader.read_u32_le();
    uint64_t cell_count_64 = static_cast<uint64_t>(columns) * rows;
    if (cell_count_64 > 10000000) throw ContentError("PUX", "PUX area too large");
    uint32_t cell_count = static_cast<uint32_t>(cell_count_64);

    expect_token(reader, "PUX texture groups");
    auto texture_groups = read_groups(reader, "PUX texture group");

    expect_token(reader, "PUX edge groups");
    auto edge_groups = read_groups(reader, "PUX edge group");

    uint32_t declared_cells = reader.read_u32_le();
    if (declared_cells != cell_count) {
        throw ContentError("PUX", "PUX cell count mismatch");
    }

    std::unordered_set<int16_t> texture_ids;
    for (const auto& g : texture_groups) {
        texture_ids.insert(g.tile_id);
    }

    std::vector<int16_t> tiles;
    tiles.reserve(cell_count);
    std::vector<std::vector<PuzzleCellLayer>> cell_layers;
    cell_layers.reserve(cell_count);

    for (uint32_t i = 0; i < cell_count; ++i) {
        uint8_t layer_count = reader.read_exact(1)[0];
        std::vector<PuzzleCellLayer> layers;
        layers.reserve(layer_count);
        uint32_t assigned_vertices = 0;

        for (uint8_t j = 0; j < layer_count; ++j) {
            uint16_t raw_tile_id = reader.read_u16_le();
            int16_t tile_id = static_cast<int16_t>(raw_tile_id);

            if (texture_ids.find(tile_id) == texture_ids.end()) {
                throw ContentError("PUX", "PUX references missing texture group");
            }

            uint32_t coverage_mask = reader.read_u32_le();
            if ((coverage_mask & ~COVERAGE_MASK) != 0) {
                throw ContentError("PUX", "PUX coverage mask invalid");
            }

            uint32_t overlap = assigned_vertices & coverage_mask;
            if (overlap != 0) {
                throw ContentError("PUX", "PUX coverage overlap");
            }
            assigned_vertices |= coverage_mask;

            layers.push_back({tile_id, coverage_mask});
        }
        
        tiles.push_back(layers.empty() ? -1 : layers.front().tile_id);
        cell_layers.push_back(std::move(layers));
    }

    uint32_t edge_count = reader.read_u32_le();
    if (edge_count > MAXIMUM_MAP_OBJECTS) {
        throw ContentError("PUX", "PUX edge record count unreasonable");
    }

    std::vector<PuzzleEdgeRecord> edge_records;
    edge_records.reserve(edge_count);
    for (uint32_t i = 0; i < edge_count; ++i) {
        PuzzleEdgeRecord rec;
        rec.group = reader.read_u32_le();
        auto sides_span = reader.read_exact(4);
        std::copy(sides_span.begin(), sides_span.end(), rec.sides.begin());
        edge_records.push_back(rec);
    }

    if (reader.remaining() != 0) {
        throw ContentError("PUX", "PUX has unexplained trailing bytes");
    }

    if (texture_groups.empty()) {
        throw ContentError("PUX", "PUX declares no texture groups");
    }

    PuzzleLayout layout;
    layout.kind = kind;
    layout.ani_path = texture_groups.front().ani_path;
    layout.columns = columns;
    layout.rows = rows;
    layout.tiles = std::move(tiles);
    layout.cell_layers = std::move(cell_layers);
    layout.texture_groups = std::move(texture_groups);
    layout.edge_groups = std::move(edge_groups);
    layout.edge_records = std::move(edge_records);
    layout.roll_speed = {0, 0};
    
    return layout;
}

PuzzleLayout PuzzleLayout::parse(std::span<const uint8_t> bytes) {
    if (bytes.size() >= 9 && std::memcmp(bytes.data(), "TqTerrain", 9) == 0) {
        return parse_pux(bytes);
    }

    SliceReader reader("PUL", bytes);
    auto kind_span = reader.read_exact(8);
    std::string kind(reinterpret_cast<const char*>(kind_span.data()), 8);
    if (kind.find("PUZZLE") != 0) {
        throw ContentError("PUL", "unexpected PUL kind");
    }

    auto ani_span = reader.read_exact(256);
    std::string ani = nul_terminated_ascii(ani_span, "PUL ani");

    int32_t columns = reader.read_i32_le();
    int32_t rows = reader.read_i32_le();
    if (columns < 0 || rows < 0) throw ContentError("PUL", "negative dimensions");

    uint64_t count = static_cast<uint64_t>(columns) * static_cast<uint64_t>(rows);
    if (count > 10000000) throw ContentError("PUL", "PUL area too large");

    std::vector<int16_t> tiles;
    tiles.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        tiles.push_back(reader.read_i16_le());
    }

    std::array<int32_t, 2> roll_speed = {0, 0};
    if (reader.remaining() >= 8) {
        roll_speed[0] = reader.read_i32_le();
        roll_speed[1] = reader.read_i32_le();
    }

    auto trailing_span = reader.read_exact(reader.remaining());
    std::vector<uint8_t> trailing_bytes(trailing_span.begin(), trailing_span.end());

    PuzzleLayout layout;
    layout.kind = kind;
    layout.ani_path = ani;
    layout.columns = columns;
    layout.rows = rows;
    layout.tiles = std::move(tiles);
    layout.roll_speed = roll_speed;
    layout.trailing_bytes = std::move(trailing_bytes);

    return layout;
}

} // namespace sandbox_data
