#pragma once

#include <memory>

#include "render/core/image_utils.h"

namespace Mizu
{

// Forward declarations
class RenderGraphBuilder;
struct RGTextureSrvRef;

class Environment
{
  public:
    static std::shared_ptr<Environment> create(const ImageUtils::Faces& faces);
    static std::shared_ptr<Environment> create(std::shared_ptr<ImageResource> cubemap);

    std::shared_ptr<ImageResource> get_cubemap() const { return m_cubemap; }
    std::shared_ptr<ImageResource> get_irradiance_map() const { return m_irradiance_map; }
    std::shared_ptr<ImageResource> get_prefiltered_environment_map() const { return m_prefiltered_environment_map; }
    std::shared_ptr<ImageResource> get_precomputed_brdf() const { return m_precomputed_brdf; }

  private:
    Environment(
        std::shared_ptr<ImageResource> cubemap,
        std::shared_ptr<ImageResource> irradiance_map,
        std::shared_ptr<ImageResource> prefiltered_environment_map,
        std::shared_ptr<ImageResource> precomputed_brdf);

    std::shared_ptr<ImageResource> m_cubemap;

    std::shared_ptr<ImageResource> m_irradiance_map;
    std::shared_ptr<ImageResource> m_prefiltered_environment_map;
    std::shared_ptr<ImageResource> m_precomputed_brdf;

    static std::shared_ptr<Environment> create_internal(std::shared_ptr<ImageResource> cubemap);

    static std::shared_ptr<ImageResource> create_irradiance_map(
        RenderGraphBuilder& builder,
        RGTextureSrvRef cubemap_ref);
    static std::shared_ptr<ImageResource> create_prefiltered_environment_map(
        RenderGraphBuilder& builder,
        RGTextureSrvRef cubemap_ref);
    static std::shared_ptr<ImageResource> create_precomputed_brdf(RenderGraphBuilder& builder);
};

} // namespace Mizu