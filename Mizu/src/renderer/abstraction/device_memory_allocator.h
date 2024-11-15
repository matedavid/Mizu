#pragma once

#include <filesystem>
#include <memory>
#include <span>

#include "renderer/cubemap.h"
#include "renderer/texture.h"

namespace Mizu {

// Forward declarations
class Texture2D;

class IDeviceMemoryAllocator {
  public:
    virtual ~IDeviceMemoryAllocator() = default;

    template <typename T>
    std::shared_ptr<T> allocate_texture(const typename T::Description& desc, const SamplingOptions& sampling) {
        static_assert(std::is_base_of<ITextureBase, T>());

        const auto resource = allocate_image_resource(T::get_image_description(desc), sampling);
        return std::make_shared<T>(resource);
    }

    std::shared_ptr<Texture2D> allocate_texture(const std::filesystem::path& path, const SamplingOptions& sampling);

    std::shared_ptr<Cubemap> allocate_cubemap(const Cubemap::Faces& faces, const SamplingOptions& sampling);
    std::shared_ptr<Cubemap> allocate_cubemap(const Cubemap::Description& desc, const SamplingOptions& sampling);

    virtual std::shared_ptr<ImageResource> allocate_image_resource(const ImageDescription& desc,
                                                                   const SamplingOptions& sampling) = 0;
    virtual std::shared_ptr<ImageResource> allocate_image_resource(const ImageDescription& desc,
                                                                   const SamplingOptions& sampling,
                                                                   const uint8_t* data,
                                                                   uint32_t size) = 0;

    void release(const std::shared_ptr<ITextureBase>& texture);
    void release(const std::shared_ptr<Cubemap>& cubemap);

    virtual void release(const std::shared_ptr<ImageResource>& resource) = 0;
};

class BaseDeviceMemoryAllocator : public IDeviceMemoryAllocator {
  public:
    static std::shared_ptr<BaseDeviceMemoryAllocator> create();
};

} // namespace Mizu