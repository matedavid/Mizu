#pragma once

#include <memory>

#include "mizu_core_module.h"

namespace Mizu
{

class AssetRegistry;
class Window;

class MIZU_CORE_API GameContext
{
  public:
    GameContext(std::shared_ptr<Window> window, std::shared_ptr<AssetRegistry> asset_registry);

    Window& get_window() const { return *m_window; }
    std::shared_ptr<Window> get_window_ptr() const { return m_window; }

    AssetRegistry& get_asset_registry() const { return *m_asset_registry; }
    std::shared_ptr<AssetRegistry> get_asset_registry_ptr() const { return m_asset_registry; }

  private:
    std::shared_ptr<Window> m_window;
    std::shared_ptr<AssetRegistry> m_asset_registry;
};

MIZU_CORE_API extern GameContext* g_game_context;

MIZU_CORE_API void create_game_context(std::shared_ptr<Window> window, std::shared_ptr<AssetRegistry> asset_registry);
MIZU_CORE_API void destroy_game_context();

} // namespace Mizu