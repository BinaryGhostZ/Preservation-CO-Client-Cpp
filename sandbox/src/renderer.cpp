#define WEBGPU_CPP_IMPLEMENTATION
#include "renderer.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include "movement.hpp"
#include "assets.hpp"
#include "character.hpp"
#include "map.hpp"
#include "minimap.hpp"
#include "labels.hpp"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <algorithm>

namespace {
    std::array<float, 2> camera(const sandbox::Map& map, const sandbox::Player& player, uint32_t width, uint32_t height) {
        auto p = map.project(std::array<float, 2>{player.position[0], player.position[1]});
        return {
            p[0] - static_cast<float>(width) * 0.5f,
            p[1] - static_cast<float>(height) * 0.5f
        };
    }
    
    std::array<float, 3> clip(std::array<float, 2> p, float z, const std::array<float, 2>& viewport) {
        return {
            p[0] / viewport[0] * 2.0f - 1.0f,
            -(p[1] / viewport[1] * 2.0f - 1.0f),
            z
        };
    }
    
    std::vector<sandbox::Vertex> quad(std::array<float, 2> p, std::array<float, 2> size, const std::array<float, 2>& viewport) {
        std::array<float, 3> tl = clip(p, 0.5f, viewport);
        std::array<float, 3> tr = clip({p[0] + size[0], p[1]}, 0.5f, viewport);
        std::array<float, 3> bl = clip({p[0], p[1] + size[1]}, 0.5f, viewport);
        std::array<float, 3> br = clip({p[0] + size[0], p[1] + size[1]}, 0.5f, viewport);
        
        return {
            sandbox::Vertex{ {tl[0], tl[1], tl[2]}, {0.0f, 0.0f} },
            sandbox::Vertex{ {tr[0], tr[1], tr[2]}, {1.0f, 0.0f} },
            sandbox::Vertex{ {bl[0], bl[1], bl[2]}, {0.0f, 1.0f} },
            sandbox::Vertex{ {bl[0], bl[1], bl[2]}, {0.0f, 1.0f} },
            sandbox::Vertex{ {tr[0], tr[1], tr[2]}, {1.0f, 0.0f} },
            sandbox::Vertex{ {br[0], br[1], br[2]}, {1.0f, 1.0f} }
        };
    }
    
    std::vector<sandbox::Vertex> quad_uv(std::array<float, 2> p, std::array<float, 2> size, std::array<float, 4> uv, const std::array<float, 2>& viewport) {
        std::array<float, 3> tl = clip(p, 0.5f, viewport);
        std::array<float, 3> tr = clip({p[0] + size[0], p[1]}, 0.5f, viewport);
        std::array<float, 3> bl = clip({p[0], p[1] + size[1]}, 0.5f, viewport);
        std::array<float, 3> br = clip({p[0] + size[0], p[1] + size[1]}, 0.5f, viewport);
        
        return {
            sandbox::Vertex{ {tl[0], tl[1], tl[2]}, {uv[0], uv[1]} },
            sandbox::Vertex{ {tr[0], tr[1], tr[2]}, {uv[0] + uv[2], uv[1]} },
            sandbox::Vertex{ {bl[0], bl[1], bl[2]}, {uv[0], uv[1] + uv[3]} },
            sandbox::Vertex{ {bl[0], bl[1], bl[2]}, {uv[0], uv[1] + uv[3]} },
            sandbox::Vertex{ {tr[0], tr[1], tr[2]}, {uv[0] + uv[2], uv[1]} },
            sandbox::Vertex{ {br[0], br[1], br[2]}, {uv[0] + uv[2], uv[1] + uv[3]} }
        };
    }
}

namespace sandbox {

Renderer::Renderer(GLFWwindow* window) : width(1366), height(768) {
    init_webgpu(window);
    create_pipelines();
    create_depth_texture();
}

Renderer::~Renderer() {
    for (auto& kv : textures) {
        if (kv.second.bind) kv.second.bind.release();
    }
    textures.clear();

    if (sprites_pipeline) sprites_pipeline.release();
    if (actor_pipeline) actor_pipeline.release();
    if (layout) layout.release();
    if (vertex_buffer) vertex_buffer.release();
    if (depth_view) depth_view.release();
    if (queue) queue.release();
    if (device) device.release();
    if (adapter) adapter.release();
    if (surface) surface.release();
    if (instance) instance.release();
}

void Renderer::init_webgpu(GLFWwindow* window) {
    wgpu::InstanceDescriptor instance_desc = {};
    instance = wgpu::createInstance(instance_desc);
    if (!instance) throw std::runtime_error("Failed to create WebGPU instance");

#if defined(_WIN32)
    wgpu::SurfaceSourceWindowsHWND hwnd_desc = {};
    hwnd_desc.chain.sType = wgpu::SType::SurfaceSourceWindowsHWND;
    hwnd_desc.hinstance = GetModuleHandle(nullptr);
    hwnd_desc.hwnd = glfwGetWin32Window(window);

    wgpu::SurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &hwnd_desc.chain;
    surface = instance.createSurface(surface_desc);
#else
    throw std::runtime_error("Unsupported platform for WebGPU surface");
#endif

    wgpu::RequestAdapterOptions adapter_opts = {};
    adapter_opts.compatibleSurface = surface;
    
    std::cout << "Requesting adapter..." << std::endl;
    adapter = instance.requestAdapter(adapter_opts);
    if (!adapter) throw std::runtime_error("Failed to acquire WebGPU adapter");

    wgpu::DeviceDescriptor device_desc = {};
    
    wgpu::UncapturedErrorCallbackInfo err_cb = {};
    err_cb.callback = [](WGPUDevice const*, WGPUErrorType, WGPUStringView message, void*, void*) {
        std::cerr << "WebGPU Error: " << (message.data ? message.data : "Unknown") << std::endl;
        exit(1);
    };
    device_desc.uncapturedErrorCallbackInfo = err_cb;

    std::cout << "Requesting device..." << std::endl;
    device = adapter.requestDevice(device_desc);
    if (!device) throw std::runtime_error("Failed to acquire WebGPU device");

    std::cout << "Getting queue..." << std::endl;
    queue = device.getQueue();

    std::cout << "Resizing..." << std::endl;
    resize(1366, 768);
    std::cout << "Init WebGPU finished." << std::endl;
}

void Renderer::resize(uint32_t new_width, uint32_t new_height) {
    if (new_width == 0 || new_height == 0) return;
    width = new_width;
    height = new_height;

    wgpu::SurfaceConfiguration config = {};
    config.device = device;
    config.format = wgpu::TextureFormat::BGRA8Unorm;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.width = width;
    config.height = height;
    config.presentMode = wgpu::PresentMode::Fifo; // AutoVsync

    std::cout << "Configuring surface..." << std::endl;
    surface.configure(config);
    std::cout << "Creating depth texture..." << std::endl;
    create_depth_texture();
    std::cout << "Depth texture created!" << std::endl;
}

void Renderer::create_depth_texture() {
    if (depth_view) {
        depth_view.release();
        depth_view = nullptr;
    }

    wgpu::TextureDescriptor depth_desc = {};
    depth_desc.usage = wgpu::TextureUsage::RenderAttachment;
    depth_desc.dimension = wgpu::TextureDimension::_2D;
    depth_desc.size = {width, height, 1};
    depth_desc.format = wgpu::TextureFormat::Depth24Plus;
    depth_desc.mipLevelCount = 1;
    depth_desc.sampleCount = 1;

    std::cout << "Calling device.createTexture..." << std::endl;
    wgpu::Texture depth_texture = device.createTexture(depth_desc);
    if (!depth_texture) {
        std::cerr << "Failed to create depth texture!" << std::endl;
        throw std::runtime_error("device.createTexture returned null");
    }

    std::cout << "Calling texture.createView..." << std::endl;
    wgpu::TextureViewDescriptor view_desc = {};
    view_desc.format = wgpu::TextureFormat::Depth24Plus;
    view_desc.dimension = wgpu::TextureViewDimension::_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = wgpu::TextureAspect::DepthOnly;
    
    depth_view = depth_texture.createView(view_desc);
    depth_texture.release();
    
    if (!depth_view) {
        std::cerr << "Failed to create depth texture view!" << std::endl;
        throw std::runtime_error("texture.createView returned null");
    }
}

void Renderer::create_pipelines() {
    // 1. Bind Group Layout
    wgpu::BindGroupLayoutEntry bgl_entries[2] = {};
    bgl_entries[0].binding = 0;
    bgl_entries[0].visibility = wgpu::ShaderStage::Fragment;
    bgl_entries[0].texture.sampleType = wgpu::TextureSampleType::Float;
    bgl_entries[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;

    bgl_entries[1].binding = 1;
    bgl_entries[1].visibility = wgpu::ShaderStage::Fragment;
    bgl_entries[1].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bgl_desc = {};
    bgl_desc.entryCount = 2;
    bgl_desc.entries = bgl_entries;
    layout = device.createBindGroupLayout(bgl_desc);

    // 2. Pipeline Layout
    wgpu::PipelineLayoutDescriptor pl_desc = {};
    pl_desc.bindGroupLayoutCount = 1;
    WGPUBindGroupLayout raw_layout = layout;
    pl_desc.bindGroupLayouts = &raw_layout;
    wgpu::PipelineLayout pipeline_layout = device.createPipelineLayout(pl_desc);

    // 3. Shader Module
    std::string wgsl_source = R"(
        struct VertexOutput {
            @builtin(position) position: vec4<f32>,
            @location(0) uv: vec2<f32>
        }

        @group(0) @binding(0) var image: texture_2d<f32>;
        @group(0) @binding(1) var image_sampler: sampler;

        @vertex fn vertex_main(
            @location(0) p: vec3<f32>,
            @location(1) uv: vec2<f32>
        ) -> VertexOutput {
            var output: VertexOutput;
            output.position = vec4<f32>(p, 1.0);
            output.uv = uv;
            return output;
        }

        @fragment fn fragment_main(input: VertexOutput) -> @location(0) vec4<f32> {
            let colour = textureSample(image, image_sampler, input.uv);
            if (colour.a < 0.1) {
                discard;
            }
            return colour;
        }
    )";
    wgpu::ShaderSourceWGSL wgsl_desc = {};
    wgsl_desc.chain.sType = wgpu::SType::ShaderSourceWGSL;
    wgsl_desc.code = WGPUStringView{wgsl_source.c_str(), WGPU_STRLEN};

    wgpu::ShaderModuleDescriptor sm_desc = {};
    sm_desc.nextInChain = &wgsl_desc.chain;
    wgpu::ShaderModule shader = device.createShaderModule(sm_desc);

    wgpu::VertexAttribute vert_attrs[2] = {};
    vert_attrs[0].format = wgpu::VertexFormat::Float32x3;
    vert_attrs[0].offset = offsetof(Vertex, position);
    vert_attrs[0].shaderLocation = 0;
    
    vert_attrs[1].format = wgpu::VertexFormat::Float32x2;
    vert_attrs[1].offset = offsetof(Vertex, uv);
    vert_attrs[1].shaderLocation = 1;

    wgpu::VertexBufferLayout vb_layout = {};
    vb_layout.arrayStride = sizeof(Vertex);
    vb_layout.stepMode = wgpu::VertexStepMode::Vertex;
    vb_layout.attributeCount = 2;
    vb_layout.attributes = vert_attrs;

    wgpu::ColorTargetState color_target = {};
    color_target.format = wgpu::TextureFormat::BGRA8Unorm; // wgpuSurfaceGetPreferredFormat
    
    wgpu::BlendState blend_state = {};
    blend_state.color.operation = wgpu::BlendOperation::Add;
    blend_state.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend_state.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend_state.alpha.operation = wgpu::BlendOperation::Add;
    blend_state.alpha.srcFactor = wgpu::BlendFactor::One;
    blend_state.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    
    color_target.blend = &blend_state;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment = {};
    fragment.module = shader;
    fragment.entryPoint = WGPUStringView{"fragment_main", WGPU_STRLEN};
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::DepthStencilState depth_stencil = {};
    depth_stencil.format = wgpu::TextureFormat::Depth24Plus;
    depth_stencil.depthWriteEnabled = WGPUOptionalBool_True;
    depth_stencil.depthCompare = wgpu::CompareFunction::LessEqual;
    depth_stencil.stencilFront.compare = wgpu::CompareFunction::Always;
    depth_stencil.stencilBack.compare = wgpu::CompareFunction::Always;

    wgpu::RenderPipelineDescriptor rp_desc = {};
    rp_desc.layout = pipeline_layout;
    rp_desc.vertex.module = shader;
    rp_desc.vertex.entryPoint = WGPUStringView{"vertex_main", WGPU_STRLEN};
    rp_desc.vertex.bufferCount = 1;
    rp_desc.vertex.buffers = &vb_layout;
    rp_desc.fragment = &fragment;
    rp_desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    rp_desc.primitive.cullMode = wgpu::CullMode::None;
    rp_desc.multisample.count = 1;
    rp_desc.multisample.mask = ~0u;
    
    rp_desc.depthStencil = &depth_stencil;

    actor_pipeline = device.createRenderPipeline(rp_desc);

    depth_stencil.depthCompare = wgpu::CompareFunction::Always;
    depth_stencil.depthWriteEnabled = WGPUOptionalBool_False;
    sprites_pipeline = device.createRenderPipeline(rp_desc);
}


void Renderer::upload_texture(const std::string& name, const Image& img) {
    if (textures.count(name) > 0 || failed_textures.count(name) > 0) return;

    int tex_width = img.width;
    int tex_height = img.height;
    const std::vector<uint8_t>& data = img.data;


    wgpu::TextureDescriptor tex_desc = {};
    tex_desc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    tex_desc.dimension = wgpu::TextureDimension::_2D;
    tex_desc.size = {static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height), 1};
    tex_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    tex_desc.mipLevelCount = 1;
    tex_desc.sampleCount = 1;
    
    wgpu::Texture wgpu_tex = device.createTexture(tex_desc);

    wgpu::TexelCopyTextureInfo dest = {};
    dest.texture = wgpu_tex;
    dest.mipLevel = 0;
    dest.origin = {0, 0, 0};
    dest.aspect = wgpu::TextureAspect::All;
    
    wgpu::TexelCopyBufferLayout layout_info = {};
    layout_info.offset = 0;
    layout_info.bytesPerRow = tex_width * 4;
    layout_info.rowsPerImage = tex_height;
    
    wgpu::Extent3D copy_size = {static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height), 1};
    queue.writeTexture(dest, data.data(), data.size(), layout_info, copy_size);

    wgpu::TextureViewDescriptor view_desc = {};
    view_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    view_desc.dimension = wgpu::TextureViewDimension::_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = wgpu::TextureAspect::All;
    
    wgpu::TextureView view = wgpu_tex.createView(view_desc);

    wgpu::SamplerDescriptor sampler_desc = {};
    sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_desc.magFilter = wgpu::FilterMode::Linear;
    sampler_desc.minFilter = wgpu::FilterMode::Linear;
    sampler_desc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    sampler_desc.maxAnisotropy = 1;
    wgpu::Sampler sampler = device.createSampler(sampler_desc);

    wgpu::BindGroupEntry bg_entries[2] = {};
    bg_entries[0].binding = 0;
    bg_entries[0].textureView = view;
    bg_entries[1].binding = 1;
    bg_entries[1].sampler = sampler;

    wgpu::BindGroupDescriptor bg_desc = {};
    bg_desc.layout = layout;
    bg_desc.entryCount = 2;
    bg_desc.entries = bg_entries;
    wgpu::BindGroup bind = device.createBindGroup(bg_desc);

    Texture t;
    t.bind = bind;
    t.size[0] = static_cast<float>(tex_width);
    t.size[1] = static_cast<float>(tex_height);
    textures[name] = t;

    wgpu_tex.release();
    view.release();
    sampler.release();
}

void Renderer::upload_texture(Assets& assets, const std::string& path) {
    if (textures.count(path) > 0 || failed_textures.count(path) > 0) return;

    try {
        auto img = assets.image(path);
        upload_texture(path, img);
    } catch (...) {
        std::cerr << "Failed to load texture " << path << std::endl;
        failed_textures.insert(path);
    }
}



void Renderer::draw(Assets& assets, const Map& map, const Character& character, const Player& player, Minimap* minimap, float time) {
    auto cam = camera(map, player, width, height);
    std::array<float, 2> viewport = {static_cast<float>(width), static_cast<float>(height)};
    
    std::vector<Draw> draws;
    
    // 1. Draw Map Sprites
    for (const auto& sprite : map.sprites) {
        float p[2] = {sprite.origin[0] - cam[0], sprite.origin[1] - cam[1]};
        float margin = (sprite.depth == -1e9f) ? 256.0f : 4096.0f;
        
        if (p[0] > viewport[0] || p[1] > viewport[1] || p[0] < -margin || p[1] < -margin) {
            continue;
        }
        
        size_t frame_index = static_cast<size_t>((time * 1000.0f / static_cast<float>(sprite.interval))) % sprite.frames.size();
        const std::string& path = sprite.frames[frame_index];
        
        upload_texture(assets, path);
        
        if (textures.find(path) == textures.end()) {
            continue;
        }
        
        const auto& texture = textures[path];
        float size[2] = {texture.size[0], texture.size[1]};
        
        if (p[0] + size[0] < 0.0f || p[1] + size[1] < 0.0f) {
            continue;
        }
        
        Draw d;
        d.texture = path;
        d.vertices = quad(std::array<float, 2>{p[0], p[1]}, std::array<float, 2>{size[0], size[1]}, viewport);
        d.depth_test = false;
        d.sort_depth = sprite.depth;
        draws.push_back(std::move(d));
    }
    
    // 2. Draw Character Parts
    std::array<float, 2> ground = {viewport[0] * 0.5f, viewport[1] * 0.5f};
    std::array<float, 2> origin = {ground[0], ground[1] - player.height};
    float actor_depth = player.position[0] + player.position[1];
    
    for (const auto& part : character.parts) {
        upload_texture(assets, part.texture);
        
        auto vertices = character.vertices(part, time, player.action, player.facing, player.progress());
        
        std::vector<Vertex> triangles;
        auto add_indices = [&](const std::vector<uint16_t>& indices) {
            for (uint16_t i : indices) {
                const auto& v = vertices[i];
                float p_x = origin[0] + v.first[0];
                float p_y = origin[1] + v.first[1];
                float z = std::clamp(0.5f + v.first[2] * 0.0005f, 0.001f, 0.999f);
                
                auto cl = clip({p_x, p_y}, z, viewport);
                
                Vertex out_v;
                out_v.position[0] = cl[0];
                out_v.position[1] = cl[1];
                out_v.position[2] = cl[2];
                out_v.uv[0] = v.second[0];
                out_v.uv[1] = v.second[1];
                triangles.push_back(out_v);
            }
        };
        add_indices(part.mesh.opaque_indices);
        add_indices(part.mesh.alpha_indices);
        
        Draw d;
        d.texture = part.texture;
        d.vertices = std::move(triangles);
        d.depth_test = true;
        d.sort_depth = actor_depth;
        draws.push_back(std::move(d));
    }

    if (textures.find("@label") == textures.end()) {
        upload_texture("@label", sandbox::labels::image(character));
    }
    
    Draw label_draw;
    label_draw.texture = "@label";
    label_draw.vertices = quad_uv(
        {std::round(origin[0] - 128.0f), std::round(origin[1] - 152.0f)},
        {256.0f, 40.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        viewport
    );
    label_draw.depth_test = false;
    label_draw.sort_depth = 1e9f;
    draws.push_back(std::move(label_draw));
    
    if (textures.find("@build") == textures.end()) {
        upload_texture("@build", sandbox::labels::build_banner());
    }
    
    if (textures.find("@build") != textures.end()) {
        Draw build_draw;
        build_draw.texture = "@build";
        build_draw.vertices = quad_uv(
            {4.0f, 3.0f},
            {textures["@build"].size[0], textures["@build"].size[1]},
            {0.0f, 0.0f, 1.0f, 1.0f},
            viewport
        );
        build_draw.depth_test = false;
        build_draw.sort_depth = 1e9f;
        draws.push_back(std::move(build_draw));
    }
    
    // Sort draws
    // Minimap UI passes
    if (minimap) {
        for (const auto& kv : minimap->images) {
            upload_texture(kv.first, kv.second);
        }
        
        auto minimap_quads = minimap->quads(map, {player.position[0], player.position[1]}, {cam[0], cam[1]}, viewport);
        for (const auto& mq : minimap_quads) {
            if (textures.find(mq.texture) == textures.end()) continue;
            
            Draw d;
            d.texture = mq.texture;
            d.vertices = quad_uv(
                std::array<float, 2>{mq.rect[0], mq.rect[1]}, 
                std::array<float, 2>{mq.rect[2], mq.rect[3]}, 
                mq.uv, 
                viewport
            );
            d.depth_test = false;
            d.sort_depth = 1e9f; // Render UI on top
            draws.push_back(std::move(d));
        }
    }

    draws.erase(std::remove_if(draws.begin(), draws.end(), [](const Draw& d) {
        return d.vertices.empty();
    }), draws.end());

    std::stable_sort(draws.begin(), draws.end(), [](const Draw& a, const Draw& b) {
        return a.sort_depth < b.sort_depth;
    });
    
    std::vector<Vertex> all_vertices;
    for (const auto& draw : draws) {
        all_vertices.insert(all_vertices.end(), draw.vertices.begin(), draw.vertices.end());
    }

    uint64_t required_size = all_vertices.size() * sizeof(Vertex);
    if (required_size > vertex_buffer_capacity) {
        vertex_buffer_capacity = (std::max<uint64_t>)(required_size, vertex_buffer_capacity * 2 + 1024);
        wgpu::BufferDescriptor buf_desc = {};
        buf_desc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        buf_desc.size = vertex_buffer_capacity;
        vertex_buffer = device.createBuffer(buf_desc);
    }
    
    if (required_size > 0) {
        queue.writeBuffer(vertex_buffer, 0, all_vertices.data(), required_size);
    }
    
    wgpu::SurfaceTexture surface_texture;
    surface.getCurrentTexture(&surface_texture);
    if (surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
        resize(width, height); // Recover surface with current dimensions
        return;
    }
    
    wgpu::TextureViewDescriptor frame_view_desc = {};
    frame_view_desc.format = wgpu::TextureFormat::BGRA8Unorm;
    frame_view_desc.dimension = wgpu::TextureViewDimension::_2D;
    frame_view_desc.baseMipLevel = 0;
    frame_view_desc.mipLevelCount = 1;
    frame_view_desc.baseArrayLayer = 0;
    frame_view_desc.arrayLayerCount = 1;
    frame_view_desc.aspect = wgpu::TextureAspect::All;
    
    WGPUTextureView raw_view = wgpuTextureCreateView(surface_texture.texture, reinterpret_cast<const WGPUTextureViewDescriptor*>(&frame_view_desc));
    wgpu::TextureView frame_view = wgpu::TextureView(raw_view);

    wgpu::CommandEncoderDescriptor enc_desc = {};
    wgpu::CommandEncoder encoder = device.createCommandEncoder(enc_desc);

    wgpu::RenderPassColorAttachment color_attach = {};
    color_attach.view = frame_view;
    color_attach.loadOp = wgpu::LoadOp::Clear;
    color_attach.storeOp = wgpu::StoreOp::Store;
    color_attach.clearValue = wgpu::Color{0.1, 0.1, 0.1, 1.0};

    wgpu::RenderPassDepthStencilAttachment depth_attach = {};
    depth_attach.view = depth_view;
    depth_attach.depthLoadOp = wgpu::LoadOp::Clear;
    depth_attach.depthStoreOp = wgpu::StoreOp::Store;
    depth_attach.depthClearValue = 1.0f;

    wgpu::RenderPassDescriptor pass_desc = {};
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attach;
    pass_desc.depthStencilAttachment = &depth_attach;

    wgpu::RenderPassEncoder pass = encoder.beginRenderPass(pass_desc);
    
    uint64_t vertex_offset = 0;
    for (size_t i = 0; i < draws.size(); ++i) {
        const auto& draw = draws[i];
        if (textures.count(draw.texture) == 0) continue;

        pass.setPipeline(draw.depth_test ? actor_pipeline : sprites_pipeline);
        pass.setBindGroup(0, textures[draw.texture].bind, 0, nullptr);
        
        uint64_t draw_size = draw.vertices.size() * sizeof(Vertex);
        pass.setVertexBuffer(0, vertex_buffer, vertex_offset, draw_size);
        pass.draw(draw.vertices.size(), 1, 0, 0);
        
        vertex_offset += draw_size;
    }

    pass.end();

    wgpu::CommandBufferDescriptor cmd_desc = {};
    wgpu::CommandBuffer command = encoder.finish(cmd_desc);
    
    queue.submit(1, &command);
    surface.present();

    // webgpu-cpp does NOT implement RAII, we must release objects!
    command.release();
    encoder.release();
    pass.release();
    frame_view.release();
    wgpuTextureRelease(surface_texture.texture);
}

} // namespace sandbox
