#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "renderer/abstraction/image_resource.h"

namespace Mizu {

template <typename DimensionsT>
struct TextureDescriptionBase {
    DimensionsT dimensions{1};
    ImageFormat format = ImageFormat::RGBA8_SRGB;
    ImageUsageBits usage = ImageUsageBits::None;

    bool generate_mips = false;
};

class ITextureBase {
  public:
    [[nodiscard]] virtual std::shared_ptr<ImageResource> get_resource() const = 0;
};

template <typename T>
class TextureBase : public ITextureBase {
  public:
    using Description = TextureDescriptionBase<T>;

    TextureBase(std::shared_ptr<ImageResource> resource);

    static ImageDescription get_image_description(const Description& desc);

    [[nodiscard]] std::shared_ptr<ImageResource> get_resource() const override { return m_resource; }

  protected:
    std::shared_ptr<ImageResource> m_resource;
};

class Texture1D : public TextureBase<glm::uvec1> {
  public:
    Texture1D(std::shared_ptr<ImageResource> resource) : TextureBase(std::move(resource)) {}
};

class Texture2D : public TextureBase<glm::uvec2> {
  public:
    Texture2D(std::shared_ptr<ImageResource> resource) : TextureBase(std::move(resource)) {}
};

class Texture3D : public TextureBase<glm::uvec3> {
  public:
    Texture3D(std::shared_ptr<ImageResource> resource) : TextureBase(std::move(resource)) {}
};

} // namespace Mizu