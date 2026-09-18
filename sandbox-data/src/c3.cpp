#include "c3.hpp"
#include "reader.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace sandbox_data {

static std::string latin1(std::span<const uint8_t> bytes) {
    std::string result;
    result.reserve(bytes.size());
    for (uint8_t b : bytes) {
        result.push_back(static_cast<char>(b));
    }
    return result;
}

C3Document C3Document::parse(std::span<const uint8_t> bytes) {
    SliceReader reader("C3 document", bytes);
    
    auto raw_header = reader.read_exact(16);
    if (raw_header.size() < 11 || std::memcmp(raw_header.data(), "MAXFILE C3 ", 11) != 0) {
        throw ContentError("C3", "header does not start with MAXFILE C3");
    }

    C3Document document;
    std::string version_full = latin1(raw_header);
    // Trim nulls and whitespace
    version_full.erase(std::find(version_full.begin(), version_full.end(), '\0'), version_full.end());
    version_full.erase(version_full.find_last_not_of(" \n\r\t") + 1);
    document.version = version_full;

    while (reader.remaining() >= 8) {
        auto tag_span = reader.read_exact(4);
        uint8_t tag[4];
        std::memcpy(tag, tag_span.data(), 4);
        
        uint32_t chunk_length = reader.read_u32_le();
        if (chunk_length > reader.remaining()) {
            throw ContentError("C3", "chunk is too large");
        }
        
        auto chunk = reader.read_exact(chunk_length);
        size_t offset = 0; // Not used because we slice

        if (std::memcmp(tag, "PHY ", 4) == 0 || std::memcmp(tag, "PHY3", 4) == 0 ||
            std::memcmp(tag, "PHY4", 4) == 0 || std::memcmp(tag, "PHY5", 4) == 0) {
            document.meshes.push_back(C3Mesh::parse(tag, chunk, offset));
        } else if (std::memcmp(tag, "MOTI", 4) == 0) {
            document.motions.push_back(C3Motion::parse(chunk, offset));
        } else {
            document.unknown_chunks.push_back(UnknownChunk{ {tag[0], tag[1], tag[2], tag[3]}, chunk_length });
        }
    }

    if (reader.remaining() != 0) {
        throw ContentError("C3", std::to_string(reader.remaining()) + " trailing bytes cannot form a chunk header");
    }

    return document;
}

static std::string read_sized_string(SliceReader& reader) {
    uint32_t length = reader.read_u32_le();
    return latin1(reader.read_exact(length));
}

static Vector3 read_vector3(SliceReader& reader) {
    return {reader.read_f32_le(), reader.read_f32_le(), reader.read_f32_le()};
}

static Vector2 read_vector2(SliceReader& reader) {
    return {reader.read_f32_le(), reader.read_f32_le()};
}

static Matrix4 read_matrix(SliceReader& reader) {
    Matrix4 m;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            m.values[r * 4 + c] = reader.read_f32_le();
        }
    }
    return m;
}

static Matrix4 quaternion_matrix(float q[4], float t[3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, zw = z * w, zx = z * x;
    float yw = y * w, yz = y * z, xw = x * w;
    Matrix4 m;
    m.values[0] = 1.0f - 2.0f * (yy + zz);
    m.values[1] = 2.0f * (xy + zw);
    m.values[2] = 2.0f * (zx - yw);
    m.values[3] = 0.0f;
    m.values[4] = 2.0f * (xy - zw);
    m.values[5] = 1.0f - 2.0f * (zz + xx);
    m.values[6] = 2.0f * (yz + xw);
    m.values[7] = 0.0f;
    m.values[8] = 2.0f * (zx + yw);
    m.values[9] = 2.0f * (yz - xw);
    m.values[10] = 1.0f - 2.0f * (yy + xx);
    m.values[11] = 0.0f;
    m.values[12] = t[0];
    m.values[13] = t[1];
    m.values[14] = t[2];
    m.values[15] = 1.0f;
    return m;
}

C3Mesh C3Mesh::parse(const uint8_t* tag, std::span<const uint8_t> bytes, size_t&) {
    SliceReader reader("C3 mesh", bytes);
    C3Mesh mesh;
    mesh.name = read_sized_string(reader);
    mesh.blend_count = reader.read_u32_le();
    mesh.opaque_vertex_count = reader.read_u32_le();
    mesh.alpha_vertex_count = reader.read_u32_le();
    uint32_t vertex_count = mesh.opaque_vertex_count + mesh.alpha_vertex_count;
    
    int morph_positions = (std::memcmp(tag, "PHY ", 4) == 0) ? 4 : 1;
    int extra_bytes = 0;
    if (std::memcmp(tag, "PHY3", 4) == 0) extra_bytes = 12;
    else if (std::memcmp(tag, "PHY5", 4) == 0) extra_bytes = 20;

    for (uint32_t i = 0; i < vertex_count; ++i) {
        C3Vertex v;
        v.position = read_vector3(reader);
        for (int m = 1; m < morph_positions; ++m) reader.skip(12);
        v.texture_coordinate = read_vector2(reader);
        v.colour = reader.read_u32_le();
        v.bone_indices[0] = reader.read_u32_le();
        v.bone_indices[1] = reader.read_u32_le();
        v.bone_weights[0] = reader.read_f32_le();
        v.bone_weights[1] = reader.read_f32_le();
        reader.skip(extra_bytes);
        mesh.vertices.push_back(v);
    }

    uint32_t opaque_triangle_count = reader.read_u32_le();
    uint32_t alpha_triangle_count = reader.read_u32_le();
    
    for (uint32_t i = 0; i < opaque_triangle_count * 3; ++i) mesh.opaque_indices.push_back(reader.read_u16_le());
    for (uint32_t i = 0; i < alpha_triangle_count * 3; ++i) mesh.alpha_indices.push_back(reader.read_u16_le());

    mesh.texture_name = read_sized_string(reader);
    mesh.bounds_minimum = read_vector3(reader);
    mesh.bounds_maximum = read_vector3(reader);
    mesh.initial_matrix = read_matrix(reader);

    mesh.texture_rows = (reader.remaining() >= 4) ? reader.read_u32_le() : 1;
    
    if (reader.remaining() >= 4) {
        uint32_t tcount = reader.read_u32_le();
        for (uint32_t i = 0; i < tcount; ++i) {
            TransparencyKey k;
            k.frame = reader.read_i32_le();
            k.alpha = reader.read_f32_le();
            reader.skip(8);
            mesh.transparency_keys.push_back(k);
        }
    }
    
    if (reader.remaining() >= 4) {
        uint32_t dcount = reader.read_u32_le();
        for (uint32_t i = 0; i < dcount; ++i) {
            DrawKey k;
            k.frame = reader.read_i32_le();
            reader.skip(4);
            k.visible = reader.read_i32_le() != 0;
            reader.skip(4);
            mesh.draw_keys.push_back(k);
        }
    }
    
    if (reader.remaining() >= 4) {
        uint32_t kcount = reader.read_u32_le();
        for (uint32_t i = 0; i < kcount; ++i) {
            TextureKey k;
            k.frame = reader.read_i32_le();
            reader.skip(8);
            k.texture = reader.read_i32_le();
            mesh.texture_keys.push_back(k);
        }
    }
    
    if (reader.remaining() >= 4) {
        auto peek = reader.read_exact(4);
        if (std::memcmp(peek.data(), "STEP", 4) == 0) reader.skip(8);
        else reader.skip(-4);
    }
    
    return mesh;
}

C3Motion C3Motion::parse(std::span<const uint8_t> bytes, size_t&) {
    SliceReader reader("C3 motion", bytes);
    C3Motion m;
    m.bone_count = reader.read_u32_le();
    m.frame_count = reader.read_u32_le();
    auto marker = reader.read_exact(4);

    if (std::memcmp(marker.data(), "KKEY", 4) == 0) {
        uint32_t count = reader.read_u32_le();
        for (uint32_t i = 0; i < count; ++i) {
            MotionKey key;
            key.frame = reader.read_u32_le();
            for (uint32_t b = 0; b < m.bone_count; ++b) key.bone_matrices.push_back(read_matrix(reader));
            m.keys.push_back(key);
        }
    } else if (std::memcmp(marker.data(), "ZKEY", 4) == 0) {
        uint32_t count = reader.read_u32_le();
        for (uint32_t i = 0; i < count; ++i) {
            MotionKey key;
            key.frame = reader.read_u16_le();
            for (uint32_t b = 0; b < m.bone_count; ++b) {
                float q[4] = {reader.read_f32_le(), reader.read_f32_le(), reader.read_f32_le(), reader.read_f32_le()};
                float t[3] = {reader.read_f32_le(), reader.read_f32_le(), reader.read_f32_le()};
                key.bone_matrices.push_back(quaternion_matrix(q, t));
            }
            m.keys.push_back(key);
        }
    } else if (std::memcmp(marker.data(), "XKEY", 4) == 0) {
        uint32_t count = reader.read_u32_le();
        for (uint32_t i = 0; i < count; ++i) {
            MotionKey key;
            key.frame = reader.read_u16_le();
            for (uint32_t b = 0; b < m.bone_count; ++b) {
                Matrix4 mat;
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) mat.values[r * 4 + c] = reader.read_f32_le();
                }
                mat.values[12] = reader.read_f32_le();
                mat.values[13] = reader.read_f32_le();
                mat.values[14] = reader.read_f32_le();
                key.bone_matrices.push_back(mat);
            }
            m.keys.push_back(key);
        }
    } else {
        reader.skip(-4);
        for (uint32_t i = 0; i < m.frame_count; ++i) {
            MotionKey key;
            key.frame = i;
            for (uint32_t b = 0; b < m.bone_count; ++b) key.bone_matrices.push_back(Matrix4()); // identity
            m.keys.push_back(key);
        }
        for (uint32_t b = 0; b < m.bone_count; ++b) {
            for (uint32_t i = 0; i < m.frame_count; ++i) {
                m.keys[i].bone_matrices[b] = read_matrix(reader);
            }
        }
    }

    m.morph_count = (reader.remaining() >= 4) ? reader.read_u32_le() : 0;
    for (uint32_t i = 0; i < m.morph_count * m.frame_count; ++i) {
        m.morph_values.push_back(reader.read_f32_le());
    }

    return m;
}

Matrix4 C3Motion::matrix_at(uint32_t bone, float frame) const {
    if (bone >= bone_count || keys.empty()) return Matrix4();
    float wrapped = (frame_count > 0) ? std::fmod(frame, static_cast<float>(frame_count)) : frame;
    if (wrapped < 0) wrapped += frame_count;
    
    const MotionKey* before = &keys[0];
    const MotionKey* after = &keys.back();
    for (const auto& key : keys) {
        if (key.frame <= wrapped) before = &key;
        if (key.frame >= wrapped) { after = &key; break; }
    }
    
    if (bone >= before->bone_matrices.size()) return Matrix4();
    Matrix4 left = before->bone_matrices[bone];
    if (bone >= after->bone_matrices.size()) return left;
    Matrix4 right = after->bone_matrices[bone];
    
    if (before->frame == after->frame) return left;
    
    float amount = (wrapped - before->frame) / (after->frame - before->frame);
    Matrix4 result;
    for (int i = 0; i < 16; ++i) {
        result.values[i] = left.values[i] + (right.values[i] - left.values[i]) * amount;
    }
    return result;
}

} // namespace sandbox_data
