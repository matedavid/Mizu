#pragma once

#include <string>

#include "render_core/resources/texture.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

class Cubemap
{
  public:
    using Description = TextureDescriptionBase<glm::uvec2>;

    struct Faces
    {
        std::string right;
        std::string left;
        std::string top;
        std::string bottom;
        std::string front;
        std::string back;
    };

    Cubemap(std::shared_ptr<ImageResource> resource) : m_resource(std::move(resource)) {}

    static std::shared_ptr<Cubemap> create(const Cubemap::Faces& faces);
    static std::shared_ptr<Cubemap> create(const Cubemap::Description& desc);

    static ImageDescription get_image_description(const Description& desc);

    std::shared_ptr<ImageResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<ImageResource> m_resource;
};

} // namespace Mizu