#include <iostream>
#include <stdexcept>
#include <string>
#include <GLFW/glfw3.h>
#include "renderer.hpp"
#include "assets.hpp"
#include "character.hpp"
#include "map.hpp"
#include "movement.hpp"
#include "minimap.hpp"
#include "audio.hpp"
#include "validation.hpp"

struct WindowData {
    sandbox::Minimap* minimap = nullptr;
    sandbox::Map* map = nullptr;
    sandbox::Player* player = nullptr;
    sandbox::Character* character = nullptr;
    sandbox::Renderer* renderer = nullptr;
    uint32_t width = 1366;
    uint32_t height = 768;
    double mouse_x = 0;
    double mouse_y = 0;
    bool held = false;
    bool held_ctrl = false;
    bool held_shift = false;
    std::optional<std::pair<int32_t, int32_t>> queued_jump;
};

void move_cursor(WindowData* data) {
    if (!data || !data->map || !data->player || !data->character) return;
    std::array<float, 2> camera = data->map->project({data->player->position[0], data->player->position[1]});
    camera[0] = std::round(camera[0] - static_cast<float>(data->width) / 2.0f);
    camera[1] = std::round(camera[1] - static_cast<float>(data->height) / 2.0f);
    
    auto unproj = data->map->unproject({static_cast<float>(data->mouse_x) + camera[0], static_cast<float>(data->mouse_y) + camera[1]});
    std::pair<int32_t, int32_t> target = {
        static_cast<int32_t>(std::round(unproj[0])),
        static_cast<int32_t>(std::round(unproj[1]))
    };
    
    if (data->held_ctrl) {
        data->player->stop();
        if (data->player->moving()) {
            data->queued_jump = target;
            return;
        }
        data->player->jump(*data->map, target, std::chrono::duration<float>(data->character->duration(sandbox::Action::Jump)));
    } else if (data->held_shift) {
        data->player->face(target);
    } else {
        data->player->go(*data->map, target);
    }
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    if (data) {
        data->mouse_x = xpos;
        data->mouse_y = ypos;
        if (data->minimap) {
            data->minimap->pointer({static_cast<float>(xpos), static_cast<float>(ypos)}, false, {static_cast<float>(data->width), static_cast<float>(data->height)});
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    if (data && button == GLFW_MOUSE_BUTTON_LEFT) {
        if (data->minimap) {
            if (data->minimap->pointer({static_cast<float>(data->mouse_x), static_cast<float>(data->mouse_y)}, action == GLFW_PRESS, {static_cast<float>(data->width), static_cast<float>(data->height)})) {
                data->held = false;
                return;
            }
        }
        data->held = (action == GLFW_PRESS);
        if (data->held) {
            move_cursor(data);
        }
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    if (data) {
        if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
            data->held_ctrl = (action != GLFW_RELEASE);
        }
        if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) {
            data->held_shift = (action != GLFW_RELEASE);
        }
        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
            else if (key == GLFW_KEY_SPACE && data->player) data->player->stop();
            else if (key == GLFW_KEY_SLASH && data->player) data->player->running = !data->player->running;
        }
    }
}

void window_focus_callback(GLFWwindow* window, int focused) {
    auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    if (data && !focused) {
        data->held = false;
        data->queued_jump = std::nullopt;
        if (data->player) data->player->stop();
    }
}

void window_size_callback(GLFWwindow* window, int width, int height) {
    auto data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    if (data) {
        data->width = static_cast<uint32_t>(width);
        data->height = static_cast<uint32_t>(height);
        if (data->renderer) {
            data->renderer->resize(data->width, data->height);
        }
    }
}

int main(int argc, char** argv) {
    std::string assets_dir;
    uint32_t body_id = 3;
    uint32_t map_id = 1002;
    bool check_assets = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--assets" && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (arg == "--body" && i + 1 < argc) {
            body_id = std::stoul(argv[++i]);
        } else if (arg == "--map" && i + 1 < argc) {
            map_id = std::stoul(argv[++i]);
        } else if (arg == "--check-assets") {
            check_assets = true;
        }
    }

    if (assets_dir.empty()) {
        std::cerr << "Usage: sandbox --assets <installation> [--body 1..4] [--map <id>] [--check-assets]" << std::endl;
        return -1;
    }

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1366, 768, "dek - offline Twin City sandbox (C++)", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    WindowData window_data;
    glfwSetWindowUserPointer(window, &window_data);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);

    try {
        std::cout << "Loading assets..." << std::endl;
        sandbox::Assets assets(assets_dir);
        std::cout << "Loading character..." << std::endl;
        sandbox::Character character = sandbox::Character::load(assets, body_id);
        std::cout << "Loading map..." << std::endl;
        sandbox::Map map = sandbox::Map::load(assets, map_id);
        
        if (check_assets) {
            std::cout << "Validating assets..." << std::endl;
            sandbox::validation::movement(map, character, {430, 380});
            return 0;
        }
        
        std::cout << "Loading minimap..." << std::endl;
        std::optional<sandbox::Minimap> minimap_opt = sandbox::Minimap::load(assets, map_id);
        if (minimap_opt) {
            window_data.minimap = &(*minimap_opt);
        }
        std::cout << "Map loaded! Initializing player..." << std::endl;
        
        sandbox::Player player(map, std::make_pair(430, 380));
        player.action = sandbox::Action::Idle;
        player.facing = 4.0f;
        
        window_data.map = &map;
        window_data.character = &character;
        window_data.player = &player;
        
        std::cout << "Loading audio..." << std::endl;
        sandbox::MovementAudio audio;
        audio.load(assets, character);
        audio.check();
        
        std::cout << "Initializing renderer..." << std::endl;
        sandbox::Renderer renderer(window);
        window_data.renderer = &renderer;
        std::cout << "Successfully initialized renderer!" << std::endl;

        float last_time = static_cast<float>(glfwGetTime());
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (window_data.width == 0 || window_data.height == 0) {
                glfwWaitEvents();
                continue;
            }
            
            float time = static_cast<float>(glfwGetTime());
            float dt = time - last_time;
            last_time = time;
            
            player.tick(dt);
            
            if (!player.moving() && window_data.queued_jump) {
                player.jump(map, *window_data.queued_jump, std::chrono::duration<float>(character.duration(sandbox::Action::Jump)));
                window_data.queued_jump = std::nullopt;
            } else if (window_data.held && !player.moving()) {
                move_cursor(&window_data);
            }
            
            audio.update(player);
            renderer.draw(assets, map, character, player, window_data.minimap, time);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception caught in main: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught in main." << std::endl;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
