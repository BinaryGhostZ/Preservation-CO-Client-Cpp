#include "character.hpp"
#include "ini.hpp"
#include "weapon_action.hpp"
#include <stdexcept>
#include <cmath>

namespace sandbox {

Character Character::load(Assets& assets, uint32_t body) {
    if (body < 1 || body > 4) {
        throw std::runtime_error("--body must be 1, 2, 3 or 4");
    }

    auto profile_cat = sandbox_data::ini::SectionCatalog::parse(assets.read("ini/character.ini"));
    auto profile = profile_cat.section("Character");
    if (!profile) throw std::runtime_error("Character profile missing");

    auto number = [&](const std::string& key) -> uint32_t {
        auto val = profile->first(key);
        if (!val) throw std::runtime_error("Missing profile " + key);
        return std::stoul(*val);
    };

    auto armor_cat = sandbox_data::ini::SectionCatalog::parse(assets.read("ini/armor.ini"));
    std::string armor_key = std::to_string(body * 1000000 + (number("Armor") / 10 * 10));
    auto entry = armor_cat.section(armor_key);
    if (!entry) throw std::runtime_error("Missing armor in armor.ini");

    auto models = sandbox_data::ini::IdPathCatalog::parse(assets.read("ini/3dobj.ini"));
    auto textures = sandbox_data::ini::IdPathCatalog::parse(assets.read("ini/3dtexture.ini"));
    auto motions = sandbox_data::ini::IdPathCatalog::parse(assets.read("ini/3dmotion.ini"));

    uint32_t model_id = std::stoul(entry->first("Mesh0").value_or("0"));
    uint32_t texture_id = std::stoul(entry->first("Texture0").value_or("0"));

    auto body_model_path = models.get(model_id);
    if (!body_model_path) throw std::runtime_error("Body model missing");
    auto doc_bytes = assets.read(*body_model_path);
    auto doc = sandbox_data::C3Document::parse(doc_bytes);

    size_t index = 0;
    bool found_body = false;
    for (size_t i = 0; i < doc.meshes.size(); ++i) {
        if (doc.meshes[i].name == "v_body") {
            index = i;
            found_body = true;
            break;
        }
    }
    if (!found_body) throw std::runtime_error("Body mesh missing");
    auto mesh = doc.meshes[index];

    uint32_t right = number("RightWeapon");
    uint32_t left = number("LeftWeapon");
    uint32_t weapon_action = sandbox_data::resolve_weapon_action(
        right ? std::optional<uint32_t>(right) : std::nullopt,
        left ? std::optional<uint32_t>(left) : std::nullopt
    );

    Action all_actions[] = {Action::Idle, Action::WalkLeft, Action::WalkRight, Action::RunLeft, Action::RunRight, Action::Jump};

    std::vector<sandbox_data::C3Document> actions;
    for (Action action : all_actions) {
        std::vector<uint32_t> keys;
        auto push_keys = [&](Action a) {
            uint32_t cat_id = action_catalog_id(a);
            keys.push_back(body * 1000000 + weapon_action * 1000 + cat_id);
            keys.push_back(1000000 + weapon_action * 1000 + cat_id);
            keys.push_back(body * 1000000 + cat_id);
            keys.push_back(1000000 + cat_id);
        };
        push_keys(action);
        if (auto paired = paired_action(action)) {
            push_keys(*paired);
        }

        std::optional<std::string> path;
        for (uint32_t k : keys) {
            if (auto p = motions.get(k)) {
                path = p;
                break;
            }
        }
        if (!path) throw std::runtime_error("Body action missing");
        actions.push_back(sandbox_data::C3Document::parse(assets.read(*path)));
    }

    std::vector<Part> parts;
    auto body_tex = textures.get(texture_id);
    if (!body_tex) throw std::runtime_error("Body texture missing");
    parts.push_back({mesh, *body_tex, std::nullopt, index});

    struct AttachmentDef {
        std::string catalog;
        uint32_t key;
        std::string attachment;
    };
    AttachmentDef att_defs[] = {
        {"ini/armet.ini", body * 1000000 + 119000 + number("Hair"), "v_armet"},
        {"ini/weapon.ini", right, "v_r_weapon"},
        {"ini/weapon.ini", left, "v_l_weapon"}
    };

    for (const auto& att : att_defs) {
        auto def_cat = sandbox_data::ini::SectionCatalog::parse(assets.read(att.catalog));
        auto att_entry = def_cat.section(std::to_string(att.key));
        if (!att_entry) throw std::runtime_error("Missing part in " + att.catalog);
        
        uint32_t model = std::stoul(att_entry->first("Mesh0").value_or("0"));
        uint32_t texture = std::stoul(att_entry->first("Texture0").value_or("0"));
        
        auto part_model_path = models.get(model);
        if (!part_model_path) throw std::runtime_error("Part model missing");
        auto part_doc = sandbox_data::C3Document::parse(assets.read(*part_model_path));
        
        size_t track = 0;
        bool found_track = false;
        for (size_t i = 0; i < doc.meshes.size(); ++i) {
            if (doc.meshes[i].name == att.attachment) {
                track = i;
                found_track = true;
                break;
            }
        }
        if (!found_track) throw std::runtime_error("Attachment missing");

        if (part_doc.meshes.empty()) throw std::runtime_error("Empty part");
        auto part_tex = textures.get(texture);
        if (!part_tex) throw std::runtime_error("Part texture missing");

        std::optional<sandbox_data::C3Motion> local_motion = std::nullopt;
        if (!part_doc.motions.empty()) local_motion = part_doc.motions.front();

        parts.push_back({part_doc.meshes.front(), *part_tex, local_motion, track});
    }

    auto timing = assets.read("ini/Action.dat");
    auto read_u32 = [&](size_t offset) -> std::optional<uint32_t> {
        if (offset + 4 > timing.size()) return std::nullopt;
        uint32_t val;
        std::memcpy(&val, timing.data() + offset, 4);
        return val; // Assume little endian for now
    };

    auto count_opt = read_u32(0);
    if (!count_opt) throw std::runtime_error("Action.dat header missing");
    uint32_t count = *count_opt;
    if (count > 1000000 || timing.size() < 4 + count * 20) throw std::runtime_error("Action.dat truncated");

    std::array<float, 6> intervals;
    for (size_t a_idx = 0; a_idx < 6; ++a_idx) {
        Action action = all_actions[a_idx];
        std::vector<uint32_t> keys;
        auto push_keys = [&](Action a) {
            uint32_t cat_id = action_catalog_id(a);
            keys.push_back(body * 1000000 + weapon_action * 1000 + cat_id);
            keys.push_back(999000000 + weapon_action * 1000 + cat_id);
            keys.push_back(body * 1000000 + 999000 + cat_id);
            keys.push_back(999999000 + cat_id);
        };
        push_keys(action);
        if (auto paired = paired_action(action)) push_keys(*paired);

        uint32_t interval = 33;
        for (uint32_t k : keys) {
            bool found = false;
            for (uint32_t i = 0; i < count; ++i) {
                if (read_u32(4 + i * 20 + 4) == k) {
                    if (auto val = read_u32(4 + i * 20 + 12)) {
                        interval = *val;
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }
        intervals[a_idx] = static_cast<float>(std::max(interval, 5u));
    }

    Character c;
    c.parts = std::move(parts);
    c.actions = std::move(actions);
    c.intervals = intervals;
    c.body_id = body;
    c.weapon_action = weapon_action;
    c.name = profile->first("Name").value_or("dek");
    c.guild = profile->first("Guild").value_or("");
    c.guild_rank = profile->first("GuildRank").value_or("");
    c.health = number("Health");
    c.maximum_health = number("MaximumHealth");
    return c;
}

std::chrono::duration<float> Character::duration(Action action) const {
    size_t idx = action_index(action);
    float frames = std::max(1u, actions[idx].motions[parts[0].track].frame_count);
    return std::chrono::duration<float>(frames * intervals[idx] / 1000.0f);
}

std::vector<std::pair<std::array<float, 3>, std::array<float, 2>>> Character::vertices(
    const Part& part, float seconds, Action action, float facing, std::optional<float> progress) const {
    
    size_t idx = action_index(action);
    const auto& track = actions[idx].motions[part.track];
    float max_frames = static_cast<float>(std::max(1u, track.frame_count));
    
    float frame = 0.0f;
    if (progress) {
        float p = std::max(0.0f, std::min(1.0f, *progress));
        frame = std::min(p * max_frames, max_frames - 1e-5f);
    } else {
        frame = std::fmod((seconds * 1000.0f / intervals[idx]), max_frames);
    }

    const auto* motion = part.local ? &(*part.local) : &track;
    auto attachment = part.local ? track.matrix_at(0, frame) : sandbox_data::Matrix4(); // Identity

    std::vector<sandbox_data::Matrix4> palette;
    palette.reserve(motion->bone_count);
    for (uint32_t b = 0; b < motion->bone_count; ++b) {
        palette.push_back(part.mesh.initial_matrix.multiply(motion->matrix_at(b, frame)).multiply(attachment));
    }

    float s = std::sin(facing);
    float c = std::cos(facing);
    const float FRAC_PI_4 = 0.78539816339f; // pi/4
    float ts = std::sin(-FRAC_PI_4);
    float tc = std::cos(-FRAC_PI_4);

    std::vector<std::pair<std::array<float, 3>, std::array<float, 2>>> result;
    result.reserve(part.mesh.vertices.size());

    for (const auto& v : part.mesh.vertices) {
        const sandbox_data::Matrix4* transform = &part.mesh.initial_matrix;
        for (int i = 0; i < 2; ++i) {
            if (v.bone_weights[i] > 0.0f && v.bone_indices[i] < palette.size()) {
                transform = &palette[v.bone_indices[i]];
                break;
            }
        }

        auto p = transform->transform_point(v.position);
        
        float rx = p.x * c - p.y * s;
        float ry = p.x * s + p.y * c;
        float rz = p.z;

        std::array<float, 3> pos = {
            rx * 0.75f,
            (ry * ts + rz * tc) * 0.75f,
            (ry * tc - rz * ts) * 0.75f
        };
        std::array<float, 2> uv = {v.texture_coordinate.x, v.texture_coordinate.y};
        result.push_back({pos, uv});
    }

    return result;
}

} // namespace sandbox
