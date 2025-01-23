#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <memory>

#include "render_core/rhi/image_resource.h"

namespace Mizu
{

// Forward declarations
class IDeviceMemoryAllocator;

template <typename DimensionsT>
struct TextureDescriptionBase
{
    DimensionsT dimensions{1};
    ImageFormat format = ImageFormat::RGBA8_SRGB;
    ImageUsageBits usage = ImageUsageBits::None;

    bool generate_mips = false;
};

class ITextureBase
{
  public:
    [[nodiscard]] virtual std::shared_ptr<ImageResource> get_resource() const = 0;
};

template <typename T, typename DimensionsT>
class TextureBase : public ITextureBase
{
  public:
    using Description = TextureDescriptionBase<DimensionsT>;

    TextureBase(std::shared_ptr<ImageResource> resource) : m_resource(std::move(resource)) {}

    [[nodiscard]] static std::shared_ptr<T> create(const Description& desc,
                                                   const SamplingOptions& sampling,
                                                   std::weak_ptr<IDeviceMemoryAllocator> allocator);

    [[nodiscard]] static std::shared_ptr<T> create(const std::filesystem::path& path,
                                                   const SamplingOptions& sampling,
                                                   std::weak_ptr<IDeviceMemoryAllocator> allocator);

    [[nodiscard]] static std::shared_ptr<T> create(const Description& desc,
                                                   const SamplingOptions& sampling,
                                                   const std::vector<uint8_t>& content,
                                                   std::weak_ptr<IDeviceMemoryAllocator> allocator);

    static ImageDescription get_image_description(const Description& desc);

    [[nodiscard]] std::shared_ptr<ImageResource> get_resource() const override { return m_resource; }

  protected:
    std::shared_ptr<ImageResource> m_resource;
};

class Texture1D : public TextureBase<Texture1D, glm::uvec1>
{
  public:
    Texture1D(std::shared_ptr<ImageResource> resource) : TextureBase(std::move(resource)) {}
};

class Texture2D : public TextureBase<Texture2D, glm::uvec2>
{
  public:
    Texture2D(std::shared_ptr<ImageResource> resource) : TextureBase(std::move(resource)) {}
};

class Texture3D : public TextureBase<Texture3D, glm::uvec3>
{
  public:
    Texture3D(std::shared_ptr<ImageResource> resource) : TextureBase(std::move(resource)) {}
};

} // namespace Mizu