#pragma once

#include "render/runtime/game_renderer.h"
#include "render_core/rhi/device.h"

#include "runtime/game_simulation.h"

namespace Mizu
{

struct GameDescription
{
    std::string name = "Mizu Game";
    Version version = Version{0, 1, 0};
    GraphicsApi graphics_api = GraphicsApi::Vulkan;
    uint32_t width = 1920;
    uint32_t height = 1080;
};

class GameMain
{
  public:
    virtual ~GameMain() = default;

    virtual GameDescription get_game_description() const = 0;
    virtual GameSimulation* create_game_simulation() const = 0;
    virtual void setup_game_renderer(GameRenderer& game_renderer) const { setup_default_game_renderer(game_renderer); }
};

} // namespace Mizu