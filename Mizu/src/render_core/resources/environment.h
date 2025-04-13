#pragma once

#include <memory>

#include "render_core/resources/cubemap.h"

namespace Mizu
{

// Forward declarations
class RenderGraphBuilder;
struct RGImageViewRef;

class Environment
{
  public:
    [[nodiscard]] static std::shared_ptr<Environment> create(const Cubemap::Faces& faces);
    [[nodiscard]] static std::shared_ptr<Environment> create(std::shared_ptr<Cubemap> cubemap);

    [[nodiscard]] std::shared_ptr<Cubemap> get_cubemap() const { return m_cubemap; }
    [[nodiscard]] std::shared_ptr<Cubemap> get_irradiance_map() const { return m_irradiance_map; }

  private:
    Environment(std::shared_ptr<Cubemap> cubemap, std::shared_ptr<Cubemap> irradiance_map);

    std::shared_ptr<Cubemap> m_cubemap;
    std::shared_ptr<Cubemap> m_irradiance_map;

    static std::shared_ptr<Environment> create_internal(std::shared_ptr<Cubemap> cubemap);

    static std::shared_ptr<Cubemap> create_irradiance_map(RenderGraphBuilder& builder, RGImageViewRef cubemap_ref);
};

} // namespace Mizu