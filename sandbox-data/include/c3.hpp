#pragma once

#include "common.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <span>

namespace sandbox_data {

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Matrix4 {
    float values[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 
        0.0f, 1.0f, 0.0f, 0.0f, 
        0.0f, 0.0f, 1.0f, 0.0f, 
        0.0f, 0.0f, 0.0f, 1.0f
    };

    Matrix4 multiply(const Matrix4& right) const {
        Matrix4 result;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int i = 0; i < 4; ++i) {
                    sum += values[row * 4 + i] * right.values[i * 4 + col];
                }
                result.values[row * 4 + col] = sum;
            }
        }
        return result;
    }

    Vector3 transform_point(const Vector3& position) const {
        return Vector3{
            position.x * values[0] + position.y * values[4] + position.z * values[8] + values[12],
            position.x * values[1] + position.y * values[5] + position.z * values[9] + values[13],
            position.x * values[2] + position.y * values[6] + position.z * values[10] + values[14]
        };
    }
};

struct UnknownChunk {
    uint8_t tag[4];
    uint32_t length;
};

struct C3Vertex {
    Vector3 position;
    Vector2 texture_coordinate;
    uint32_t colour;
    uint32_t bone_indices[2];
    float bone_weights[2];
};

struct TransparencyKey {
    int32_t frame;
    float alpha;
};

struct DrawKey {
    int32_t frame;
    bool visible;
};

struct TextureKey {
    int32_t frame;
    int32_t texture;
};

struct C3Motion;

struct C3Mesh {
    std::string name;
    uint32_t blend_count;
    uint32_t opaque_vertex_count;
    uint32_t alpha_vertex_count;
    std::vector<C3Vertex> vertices;
    std::vector<uint16_t> opaque_indices;
    std::vector<uint16_t> alpha_indices;
    std::string texture_name;
    Vector3 bounds_minimum;
    Vector3 bounds_maximum;
    Matrix4 initial_matrix;
    uint32_t texture_rows;
    std::vector<TransparencyKey> transparency_keys;
    std::vector<DrawKey> draw_keys;
    std::vector<TextureKey> texture_keys;

    static C3Mesh parse(const uint8_t* tag, std::span<const uint8_t> bytes, size_t& offset);
};

struct MotionKey {
    uint32_t frame;
    std::vector<Matrix4> bone_matrices;
};

struct C3Motion {
    uint32_t bone_count;
    uint32_t frame_count;
    std::vector<MotionKey> keys;
    uint32_t morph_count;
    std::vector<float> morph_values;

    static C3Motion parse(std::span<const uint8_t> bytes, size_t& offset);
    Matrix4 matrix_at(uint32_t bone, float frame) const;
};

struct C3Document {
    std::string version;
    std::vector<C3Mesh> meshes;
    std::vector<C3Motion> motions;
    std::vector<UnknownChunk> unknown_chunks;

    static C3Document parse(std::span<const uint8_t> bytes);
};

} // namespace sandbox_data
