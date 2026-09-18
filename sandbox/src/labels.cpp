#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "labels.hpp"
#include <fstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace sandbox {
namespace labels {

const float FONT_SIZE = 12.0f;
const std::array<uint8_t, 3> GUILD_COLOUR = {255, 255, 0};
const std::array<uint8_t, 3> RANK_COLOUR = {145, 185, 255};
const std::string BUILD_TEXT = "[Preservation] Skeleton test build by @digitalm1nd | Discord: _dek | discord.gg/CvKPXEHYRY";

struct FontData {
    std::vector<uint8_t> buffer;
    stbtt_fontinfo info;
    bool loaded = false;
};

FontData& get_font() {
    static FontData font;
    if (!font.loaded) {
        std::ifstream file("C:\\Windows\\Fonts\\arial.ttf", std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Could not load C:\\Windows\\Fonts\\arial.ttf");
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        font.buffer.resize(size);
        if (!file.read(reinterpret_cast<char*>(font.buffer.data()), size)) {
            throw std::runtime_error("Failed to read font file");
        }
        if (!stbtt_InitFont(&font.info, font.buffer.data(), stbtt_GetFontOffsetForIndex(font.buffer.data(), 0))) {
            throw std::runtime_error("Failed to initialize stb_truetype");
        }
        font.loaded = true;
    }
    return font;
}

float measure_width(const std::string& text) {
    if (text.empty()) return 0.0f;
    auto& font = get_font();
    float scale = stbtt_ScaleForPixelHeight(&font.info, FONT_SIZE);
    float width = 0.0f;
    for (char c : text) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font.info, c, &advance, &lsb);
        width += advance * scale;
    }
    return width;
}

void draw_text(Image& image, const std::vector<std::pair<std::string, std::array<uint8_t, 3>>>& spans, std::array<float, 2> origin) {
    auto& font = get_font();
    float scale = stbtt_ScaleForPixelHeight(&font.info, FONT_SIZE);
    
    for (int shadow = 1; shadow >= 0; --shadow) {
        float cursor = std::round(origin[0]);
        for (const auto& span : spans) {
            std::array<uint8_t, 3> colour = shadow ? std::array<uint8_t, 3>{0, 0, 0} : span.second;
            int offset = shadow;
            
            for (char c : span.first) {
                int advance, lsb;
                stbtt_GetCodepointHMetrics(&font.info, c, &advance, &lsb);
                
                int c_x1, c_y1, c_x2, c_y2;
                stbtt_GetCodepointBitmapBox(&font.info, c, scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);
                
                int w = c_x2 - c_x1;
                int h = c_y2 - c_y1;
                
                if (w > 0 && h > 0) {
                    std::vector<uint8_t> bitmap(w * h);
                    stbtt_MakeCodepointBitmap(&font.info, bitmap.data(), w, h, w, scale, scale, c);
                    
                    int basex = static_cast<int>(cursor) + c_x1 + offset;
                    // origin[1] is top, adding 12 for baseline
                    int basey = static_cast<int>(std::round(origin[1])) + 12 + c_y1 + offset;
                    
                    for (int row = 0; row < h; ++row) {
                        for (int col = 0; col < w; ++col) {
                            int x = basex + col;
                            int y = basey + row;
                            if (x < 0 || y < 0 || x >= image.width || y >= image.height) continue;
                            
                            uint8_t alpha = bitmap[row * w + col];
                            if (alpha == 0) continue;
                            
                            int idx = (y * image.width + x) * 4;
                            float src_a = alpha / 255.0f;
                            float dst_a = image.data[idx + 3] / 255.0f;
                            float out_a = src_a + dst_a * (1.0f - src_a);
                            
                            if (out_a > 0.0f) {
                                for (int i = 0; i < 3; ++i) {
                                    float src_c = colour[i] / 255.0f;
                                    float dst_c = image.data[idx + i] / 255.0f;
                                    float out_c = (src_c * src_a + dst_c * dst_a * (1.0f - src_a)) / out_a;
                                    image.data[idx + i] = static_cast<uint8_t>(std::clamp(out_c * 255.0f, 0.0f, 255.0f));
                                }
                                image.data[idx + 3] = static_cast<uint8_t>(std::clamp(out_a * 255.0f, 0.0f, 255.0f));
                            }
                        }
                    }
                }
                
                cursor += advance * scale;
            }
        }
    }
}

Image image(const Character& character) {
    Image img;
    img.width = 256;
    img.height = 40;
    img.channels = 4;
    img.data.resize(256 * 40 * 4, 0);
    
    std::string separator = (character.guild.empty() || character.guild_rank.empty()) ? "" : " ";
    float guild_width = measure_width(character.guild) + measure_width(separator) + measure_width(character.guild_rank);
    
    draw_text(img, {
        {character.guild, GUILD_COLOUR},
        {separator, GUILD_COLOUR},
        {character.guild_rank, RANK_COLOUR}
    }, {(256.0f - guild_width) * 0.5f, 0.0f});
    
    draw_text(img, {
        {character.name, {255, 255, 255}}
    }, {(256.0f - measure_width(character.name)) * 0.5f, 13.0f});
    
    for (int y = 28; y < 34; ++y) {
        for (int x = 102; x < 154; ++x) {
            int idx = (y * 256 + x) * 4;
            img.data[idx] = 0;
            img.data[idx + 1] = 0;
            img.data[idx + 2] = 0;
            img.data[idx + 3] = 255;
        }
    }
    
    float fraction = static_cast<float>(character.health) / static_cast<float>(std::max({character.maximum_health, character.health, 1u}));
    int filled_width = static_cast<int>(std::round(50.0f * fraction));
    
    for (int y = 29; y < 33; ++y) {
        for (int x = 103; x < 103 + filled_width; ++x) {
            int idx = (y * 256 + x) * 4;
            img.data[idx] = 34;
            img.data[idx + 1] = 190;
            img.data[idx + 2] = 62;
            img.data[idx + 3] = 255;
        }
    }
    
    return img;
}

Image build_banner() {
    float w = std::ceil(measure_width(BUILD_TEXT)) + 2.0f;
    Image img;
    img.width = static_cast<int>(w);
    img.height = 16;
    img.channels = 4;
    img.data.resize(img.width * 16 * 4, 0);
    
    draw_text(img, {
        {BUILD_TEXT, GUILD_COLOUR}
    }, {0.0f, 0.0f});
    
    return img;
}

} // namespace labels
} // namespace sandbox
