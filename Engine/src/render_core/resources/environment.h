#pragma once

#include <memory>

#include "render_core/resources/cubemap.h"

namespace Mizu
{

// Forward declarations
class Texture2D;
class RenderGraphBuilder;
struct RGImageViewRef;

class Environment
{
  public:
    [[nodiscard]] static std::shared_ptr<Environment> create(const Cubemap::Faces& faces);
    [[nodiscard]] static std::shared_ptr<Environment> create(std::shared_ptr<Cubemap> cubemap);

    [[nodiscard]] std::shared_ptr<Cubemap> get_cubemap() const { return m_cubemap; }
    [[nodiscard]] std::shared_ptr<Cubemap> get_irradiance_map() const { return m_irradiance_map; }
    [[nodiscard]] std::shared_ptr<Cubemap> get_prefiltered_environment_map() const
    {
        return m_prefiltered_environment_map;
    }
    [[nodiscard]] std::shared_ptr<Texture2D> get_precomputed_brdf() const { return m_precomputed_brdf; }

  private:
    Environment(std::shared_ptr<Cubemap> cubemap,
                std::shared_ptr<Cubemap> irradiance_map,
                std::shared_ptr<Cubemap> prefiltered_environment_map,
                std::shared_ptr<Texture2D> precomputed_brdf);

    std::shared_ptr<Cubemap> m_cubemap;

    std::shared_ptr<Cubemap> m_irradiance_map;
    std::shared_ptr<Cubemap> m_prefiltered_environment_map;
    std::shared_ptr<Texture2D> m_precomputed_brdf;

    static std::shared_ptr<Environment> create_internal(std::shared_ptr<Cubemap> cubemap);

    static std::shared_ptr<Cubemap> create_irradiance_map(RenderGraphBuilder& builder, RGImageViewRef cubemap_ref);
    static std::shared_ptr<Cubemap> create_prefiltered_environment_map(RenderGraphBuilder& builder,
                                                                       RGImageViewRef cubemap_ref);
    static std::shared_ptr<Texture2D> create_precomputed_brdf(RenderGraphBuilder& builder);
};

} // namespace Mizu