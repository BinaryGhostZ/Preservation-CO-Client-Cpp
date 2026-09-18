#include "map.hpp"
#include "ini.hpp"
#include <stdexcept>
#include <algorithm>

#undef max

namespace sandbox {

static std::vector<std::string> get_frames(const sandbox_data::ini::SectionCatalog& ani, const std::string& name) {
    auto section_opt = ani.section(name);
    if (!section_opt) {
        throw std::runtime_error("Missing ANI section " + name);
    }
    
    std::string count_str = section_opt->first("FrameAmount").value_or("1");
    size_t count = std::stoull(count_str);
    if (count == 0 || count > 4096) {
        throw std::runtime_error("invalid ANI frame count");
    }
    
    std::vector<std::string> frames;
    frames.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto path_opt = section_opt->first("Frame" + std::to_string(i));
        if (!path_opt) {
            throw std::runtime_error("Missing " + name + "/Frame" + std::to_string(i));
        }
        std::string p = *path_opt;
        std::replace(p.begin(), p.end(), '\\', '/');
        frames.push_back(p);
    }
    
    return frames;
}

Map Map::load(Assets& assets, uint32_t id) {
    auto catalog_bytes = assets.read("ini/GameMap.dat");
    auto catalog = sandbox_data::GameMapCatalog::parse(catalog_bytes);
    
    auto record = catalog.get(id);
    if (!record) {
        throw std::runtime_error("Map is not in GameMap.dat");
    }
    
    auto data_bytes = assets.read(record->dmap_path.as_string());
    auto data = sandbox_data::DMap::parse(data_bytes);
    
    auto puzzle_bytes = assets.read(data.puzzle_path.as_string());
    auto puzzle = sandbox_data::PuzzleLayout::parse(puzzle_bytes);
    
    if (puzzle.is_layered() || !data.additional_puzzle_layers.empty()) {
        throw std::runtime_error("This minimal renderer supports flat PUL maps, not layered PUX maps");
    }
    
    float tile = static_cast<float>(record->tile_size);
    if (tile <= 0.0f) {
        throw std::runtime_error("invalid map tile size");
    }
    
    float extent[2] = {
        static_cast<float>(puzzle.columns) * tile,
        static_cast<float>(puzzle.rows) * tile
    };
    
    std::vector<Sprite> sprites;
    
    auto ani_bytes = assets.read(puzzle.ani_path.as_string());
    auto ani = sandbox_data::ini::SectionCatalog::parse(ani_bytes);
    
    for (size_t i = 0; i < puzzle.tiles.size(); ++i) {
        int16_t tile_id = puzzle.tiles[i];
        if (tile_id < 0) continue;
        
        std::string section_name = "Puzzle" + std::to_string(tile_id);
        auto frames = get_frames(ani, section_name);
        
        uint32_t col = i % puzzle.columns;
        uint32_t row = i / puzzle.columns;
        
        Sprite sprite;
        sprite.frames = std::move(frames);
        sprite.origin[0] = static_cast<float>(col) * tile;
        sprite.origin[1] = static_cast<float>(row) * tile;
        sprite.depth = -1e9f; // -INFINITY approx
        sprite.interval = 100;
        
        sprites.push_back(std::move(sprite));
    }
    
    auto project_pt = [&](float x, float y) -> std::array<float, 2> {
        return {
            (x - y) * 32.0f + extent[0] * 0.5f,
            (x + y - static_cast<float>(data.height - 1)) * 16.0f + extent[1] * 0.5f
        };
    };
    
    for (size_t i = 0; i < data.terrain_objects.size(); ++i) {
        const auto& obj = data.terrain_objects[i];
        if (!obj.is_renderable()) continue;
        
        auto obj_ani_bytes = assets.read(obj.ani_path.as_string());
        auto obj_ani = sandbox_data::ini::SectionCatalog::parse(obj_ani_bytes);
        
        auto p = project_pt(static_cast<float>(obj.x), static_cast<float>(obj.y));
        
        Sprite sprite;
        sprite.frames = get_frames(obj_ani, obj.section);
        sprite.origin[0] = p[0] - static_cast<float>(obj.offset_x);
        sprite.origin[1] = p[1] - static_cast<float>(obj.offset_y);
        sprite.depth = static_cast<float>(obj.x + obj.y);
        sprite.interval = obj.frame_interval_ms > 0 ? obj.frame_interval_ms : 1;
        
        sprites.push_back(std::move(sprite));
    }
    
    for (size_t i = 0; i < data.scene_objects.size(); ++i) {
        const auto& obj = data.scene_objects[i];
        auto scene_bytes = assets.read(obj.scene_path.as_string());
        auto scene = sandbox_data::MapScene::parse(scene_bytes);
        
        for (size_t j = 0; j < scene.parts.size(); ++j) {
            const auto& part = scene.parts[j];
            int32_t x = obj.x + part.scene_offset_x;
            int32_t y = obj.y + part.scene_offset_y;
            
            auto p = project_pt(static_cast<float>(x), static_cast<float>(y));
            
            auto part_ani_bytes = assets.read(part.ani_path.as_string());
            auto part_ani = sandbox_data::ini::SectionCatalog::parse(part_ani_bytes);
            
            Sprite sprite;
            sprite.frames = get_frames(part_ani, part.section);
            sprite.origin[0] = p[0] + static_cast<float>(part.offset_x);
            sprite.origin[1] = p[1] + static_cast<float>(part.offset_y);
            sprite.depth = static_cast<float>(x + y);
            sprite.interval = part.frame_interval_ms > 0 ? part.frame_interval_ms : 1;
            
            sprites.push_back(std::move(sprite));
        }
        
        sandbox_data::apply_fixed_scene_layers(data, obj.x, obj.y, scene);
    }
    
    std::sort(sprites.begin(), sprites.end(), [](const Sprite& a, const Sprite& b) {
        return a.depth < b.depth;
    });
    
    Map map;
    map.id = id;
    map.data = std::move(data);
    map.sprites = std::move(sprites);
    map.extent[0] = extent[0];
    map.extent[1] = extent[1];
    
    return map;
}

std::array<float, 2> Map::project(const std::array<float, 2>& p) const {
    return {
        (p[0] - p[1]) * 32.0f + extent[0] * 0.5f,
        (p[0] + p[1] - static_cast<float>(data.height - 1)) * 16.0f + extent[1] * 0.5f
    };
}

std::array<float, 2> Map::unproject(const std::array<float, 2>& p) const {
    float d = (p[0] - extent[0] * 0.5f) / 32.0f;
    float s = (p[1] - extent[1] * 0.5f) / 16.0f + static_cast<float>(data.height - 1);
    return {
        (s + d) * 0.5f,
        (s - d) * 0.5f
    };
}

bool Map::walkable(const std::array<int32_t, 2>& p) const {
    if (p[0] < 0 || p[1] < 0) return false;
    auto cell = data.cell(static_cast<uint32_t>(p[0]), static_cast<uint32_t>(p[1]));
    if (cell && cell->access == 0) {
        return true;
    }
    return false;
}

} // namespace sandbox
