#pragma once

#include <webgpu/webgpu.hpp>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace sandbox {

struct Vertex {
    float position[3];
    float uv[2];
};

struct Draw {
    std::string texture;
    std::vector<Vertex> vertices;
    bool depth_test;
    float sort_depth;
};

struct Texture {
    wgpu::BindGroup bind = nullptr;
    float size[2];
};

class Renderer {
public:
    wgpu::Instance instance = nullptr;
    wgpu::Surface surface = nullptr;
    wgpu::Adapter adapter = nullptr;
    wgpu::Device device = nullptr;
    wgpu::Queue queue = nullptr;
    
    wgpu::BindGroupLayout layout = nullptr;
    wgpu::RenderPipeline sprites_pipeline = nullptr;
    wgpu::RenderPipeline actor_pipeline = nullptr;
    wgpu::TextureView depth_view = nullptr;
    wgpu::Buffer vertex_buffer = nullptr;
    uint64_t vertex_buffer_capacity = 0;

    uint32_t width;
    uint32_t height;

    std::unordered_map<std::string, Texture> textures;
    std::unordered_set<std::string> failed_textures;

    Renderer(GLFWwindow* window);
    ~Renderer();

    void resize(uint32_t new_width, uint32_t new_height);
    void draw(class Assets& assets, const class Map& map, const class Character& character, const class Player& player, class Minimap* minimap, float time);

private:
    void upload_texture(const std::string& name, const struct Image& img);
    void upload_texture(class Assets& assets, const std::string& path);
    void init_webgpu(GLFWwindow* window);
    void create_pipelines();
    void create_depth_texture();
};

} // namespace sandbox
