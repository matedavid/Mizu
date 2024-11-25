#pragma once

#include <string>

#include "renderer/abstraction/image_resource.h"
#include "renderer/texture.h"

namespace Mizu {

class Cubemap {
  public:
    using Description = TextureDescriptionBase<glm::uvec2>;

    struct Faces {
        std::string right;
        std::string left;
        std::string top;
        std::string bottom;
        std::string front;
        std::string back;
    };

    Cubemap(std::shared_ptr<ImageResource> resource) : m_resource(std::move(resource)) {}

    [[nodiscard]] static std::shared_ptr<Cubemap> create(const Cubemap::Faces& faces,
                                                         const SamplingOptions& sampling,
                                                         std::weak_ptr<IDeviceMemoryAllocator> allocator);
    [[nodiscard]] static std::shared_ptr<Cubemap> create(const Cubemap::Description& desc,
                                                         const SamplingOptions& sampling,
                                                         std::weak_ptr<IDeviceMemoryAllocator> allocator);

    static ImageDescription get_image_description(const Description& desc);

    [[nodiscard]] std::shared_ptr<ImageResource> get_resource() const { return m_resource; }

  private:
    std::shared_ptr<ImageResource> m_resource;
};

} // namespace Mizu