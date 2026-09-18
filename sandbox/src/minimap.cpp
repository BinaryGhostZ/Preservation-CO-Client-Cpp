#include "minimap.hpp"
#include "ini.hpp"
#include <stdexcept>
#include <algorithm>

namespace sandbox {

namespace {
    Image stitch(const std::vector<Image>& frames) {
        if (frames.size() == 1) {
            return frames[0];
        }
        if (frames.size() != 4) {
            throw std::runtime_error("stitch requires exactly 1 or 4 frames");
        }

        int left = std::max(frames[0].width, frames[2].width);
        int top = std::max(frames[0].height, frames[1].height);
        int total_width = left + std::max(frames[1].width, frames[3].width);
        int total_height = top + std::max(frames[2].height, frames[3].height);

        Image result;
        result.width = total_width;
        result.height = total_height;
        result.channels = 4;
        result.data.resize(total_width * total_height * 4, 0);

        auto replace = [&](const Image& frame, int dest_x, int dest_y) {
            for (int y = 0; y < frame.height; ++y) {
                for (int x = 0; x < frame.width; ++x) {
                    int src_idx = (y * frame.width + x) * frame.channels;
                    int dst_idx = ((dest_y + y) * total_width + (dest_x + x)) * 4;
                    for (int c = 0; c < frame.channels; ++c) {
                        result.data[dst_idx + c] = frame.data[src_idx + c];
                    }
                    if (frame.channels == 3) {
                        result.data[dst_idx + 3] = 255;
                    }
                }
            }
        };

        replace(frames[0], 0, 0);
        replace(frames[1], left, 0);
        replace(frames[2], 0, top);
        replace(frames[3], left, top);

        return result;
    }

    std::array<float, 2> project_cell(std::array<float, 2> cell, std::array<uint32_t, 2> grid, std::array<float, 2> drawable, std::array<float, 2> raster) {
        float world[2] = {
            (cell[0] - cell[1]) * 32.0f + static_cast<float>(grid[0]) * 32.0f,
            (cell[0] + cell[1]) * 16.0f + 16.0f
        };
        float origin[2] = {
            static_cast<float>(grid[0]) * 32.0f - drawable[0] * 0.5f,
            static_cast<float>(grid[1]) * 16.0f - drawable[1] * 0.5f + 16.0f - ((grid[1] % 2 == 0) ? 16.0f : 0.0f)
        };
        return {
            (world[0] - origin[0]) / drawable[0] * raster[0],
            (world[1] - origin[1]) / drawable[1] * raster[1]
        };
    }

    std::array<float, 2> minimap_project(const Map& map, std::array<float, 2> cell, std::array<float, 2> raster) {
        return project_cell(cell, {map.data.width, map.data.height}, {map.extent[0], map.extent[1]}, raster);
    }

    std::array<float, 4> crop(std::array<float, 2> center, std::array<float, 2> raster, std::array<float, 2> destination) {
        float width = std::min(destination[0], raster[0]);
        float height = std::min(destination[1], raster[1]);
        return {
            std::clamp(center[0] - width * 0.5f, 0.0f, raster[0] - width),
            std::clamp(center[1] - height * 0.5f, 0.0f, raster[1] - height),
            width,
            height
        };
    }

    std::optional<Quad> clipped(const std::string& texture, std::array<float, 4> rect, std::array<float, 4> bounds) {
        float left = std::max(rect[0], bounds[0]);
        float top = std::max(rect[1], bounds[1]);
        float right = std::min(rect[0] + rect[2], bounds[0] + bounds[2]);
        float bottom = std::min(rect[1] + rect[3], bounds[1] + bounds[3]);
        
        if (right <= left || bottom <= top) {
            return std::nullopt;
        }
        
        return Quad{
            texture,
            {left, top, right - left, bottom - top},
            {
                (left - rect[0]) / rect[2],
                (top - rect[1]) / rect[3],
                (right - left) / rect[2],
                (bottom - top) / rect[3]
            }
        };
    }
}

std::optional<Minimap> Minimap::load(Assets& assets, uint32_t map_id) {
    auto catalog_bytes = assets.read("ani/MiniMap.Ani");
    auto catalog = sandbox_data::ini::SectionCatalog::parse(catalog_bytes);
    
    auto section = catalog.section(std::to_string(map_id));
    if (!section) return std::nullopt;
    
    size_t count = std::stoull(section->first("FrameAmount").value_or("1"));
    if (count == 0) return std::nullopt;
    if (count != 1 && count != 4) throw std::runtime_error("Unsupported minimap frame count");
    
    std::vector<Image> frames;
    for (size_t i = 0; i < count; ++i) {
        auto frame_path = section->first("Frame" + std::to_string(i));
        if (!frame_path) throw std::runtime_error("Missing minimap frame");
        frames.push_back(assets.image(*frame_path));
    }
    
    Image composite = stitch(frames);
    std::array<float, 2> size = {static_cast<float>(composite.width), static_cast<float>(composite.height)};
    
    Image crop_img = composite;
    Image fit_img = composite;
    
    for (size_t i = 3; i < crop_img.data.size(); i += 4) {
        crop_img.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(crop_img.data[i]) * 170) / 255);
    }
    for (size_t i = 3; i < fit_img.data.size(); i += 4) {
        fit_img.data[i] = static_cast<uint8_t>((static_cast<uint16_t>(fit_img.data[i]) * 153) / 255);
    }
    
    auto hero_sec = catalog.section("hero");
    if (!hero_sec) throw std::runtime_error("Missing minimap hero section");
    auto hero_path = hero_sec->first("Frame0");
    if (!hero_path) throw std::runtime_error("Missing minimap hero frame");
    
    Minimap minimap;
    minimap.images.push_back({"@minimap-crop", std::move(crop_img)});
    minimap.images.push_back({"@minimap-fit", std::move(fit_img)});
    minimap.images.push_back({"@minimap-hero", assets.image(*hero_path)});
    
    auto controls_bytes = assets.read("ani/Control.Ani");
    auto controls = sandbox_data::ini::SectionCatalog::parse(controls_bytes);
    
    std::vector<std::pair<std::string, std::array<std::string, 2>>> ctrl_sections = {
        {"Button00", {"@minimap-plus", "@minimap-plus-down"}},
        {"Button01", {"@minimap-minus", "@minimap-minus-down"}},
        {"Button02", {"@minimap-expand", "@minimap-expand-down"}}
    };
    
    for (const auto& [sec_name, keys] : ctrl_sections) {
        auto sec = controls.section(sec_name);
        if (!sec) throw std::runtime_error("Missing minimap control " + sec_name);
        for (size_t i = 0; i < keys.size(); ++i) {
            auto frame_path = sec->first("Frame" + std::to_string(i));
            if (!frame_path) throw std::runtime_error("Missing minimap control frame");
            minimap.images.push_back({keys[i], assets.image(*frame_path)});
        }
    }
    
    minimap.size = size;
    minimap.single = (count == 1);
    minimap.fitted = false;
    minimap.expanded = false;
    minimap.pressed = std::nullopt;
    
    return minimap;
}

bool Minimap::pointer(std::array<float, 2> position, bool down, std::array<float, 2> viewport) {
    float hit_zones[] = {viewport[0] - 40.0f, viewport[0] - 20.0f};
    std::optional<size_t> hit = std::nullopt;
    for (size_t i = 0; i < 2; ++i) {
        if (position[0] >= hit_zones[i] && position[0] < hit_zones[i] + 16.0f &&
            position[1] >= 4.0f && position[1] < 20.0f) {
            hit = i;
            break;
        }
    }

    if (down) {
        pressed = hit;
        return hit.has_value();
    }

    auto was_pressed = pressed;
    pressed = std::nullopt;

    if (was_pressed && was_pressed == hit) {
        if (hit == 0) {
            fitted = !fitted;
        } else {
            expanded = !expanded;
        }
    }
    return was_pressed.has_value();
}

std::vector<Quad> Minimap::quads(const Map& map, std::array<float, 2> player, std::array<float, 2> camera, std::array<float, 2> viewport) const {
    std::array<float, 2> current_size = (expanded || single) ? size : std::array<float, 2>{170.0f, 128.0f};
    std::array<float, 4> rect = {viewport[0] - current_size[0], 0.0f, current_size[0], current_size[1]};
    
    bool is_fitted = fitted || expanded;
    std::array<float, 4> source;
    if (is_fitted || single) {
        source = {0.0f, 0.0f, size[0], size[1]};
    } else {
        std::array<float, 2> center = map.unproject({camera[0] + viewport[0] * 0.5f, camera[1] + viewport[1] * 0.5f});
        source = crop(minimap_project(map, center, size), size, current_size);
    }
    
    std::vector<Quad> out_quads;
    out_quads.push_back({
        (is_fitted && !single) ? "@minimap-fit" : "@minimap-crop",
        rect,
        {
            source[0] / size[0],
            source[1] / size[1],
            source[2] / size[0],
            source[3] / size[1]
        }
    });
    
    std::array<float, 2> point = minimap_project(map, player, size);
    
    const Image* hero = nullptr;
    for (const auto& kv : images) {
        if (kv.first == "@minimap-hero") { hero = &kv.second; break; }
    }
    
    if (hero) {
        std::array<float, 4> marker = {
            rect[0] + (point[0] - source[0]) * rect[2] / source[2] - static_cast<float>(hero->width) * 0.5f,
            rect[1] + (point[1] - source[1]) * rect[3] / source[3] - static_cast<float>(hero->height) * 0.5f,
            static_cast<float>(hero->width),
            static_cast<float>(hero->height)
        };
        auto hq = clipped("@minimap-hero", marker, rect);
        if (hq) out_quads.push_back(*hq);
    }
    
    std::array<std::array<std::string, 2>, 2> buttons = {
        fitted ? std::array<std::string, 2>{"@minimap-minus", "@minimap-minus-down"} : std::array<std::string, 2>{"@minimap-plus", "@minimap-plus-down"},
        std::array<std::string, 2>{"@minimap-expand", "@minimap-expand-down"}
    };
    
    for (size_t i = 0; i < buttons.size(); ++i) {
        const std::string& key = buttons[i][(pressed == i) ? 1 : 0];
        
        const Image* btn_img = nullptr;
        for (const auto& kv : images) {
            if (kv.first == key) { btn_img = &kv.second; break; }
        }
        
        if (btn_img) {
            out_quads.push_back({
                key,
                {viewport[0] - 40.0f + static_cast<float>(i) * 20.0f, 4.0f, static_cast<float>(btn_img->width), static_cast<float>(btn_img->height)},
                {0.0f, 0.0f, 1.0f, 1.0f}
            });
        }
    }
    
    return out_quads;
}

} // namespace sandbox
