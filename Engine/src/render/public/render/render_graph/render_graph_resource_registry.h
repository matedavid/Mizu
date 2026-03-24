#pragma once

#include <memory>
#include <unordered_map>

namespace Mizu
{

class BufferResource;
class ImageResource;
struct BufferDescription;
struct ImageDescription;

class RenderGraphResourceRegistry
{
  public:
    std::shared_ptr<BufferResource> create_buffer(const BufferDescription& desc, uint64_t offset);
    std::shared_ptr<ImageResource> create_image(const ImageDescription& desc, uint64_t offset);

    void purge();
    void reset();

  private:
    struct ResourceInfo
    {
        uint64_t frames_non_used = 0;
    };

    struct BufferResourceInfo : public ResourceInfo
    {
        std::shared_ptr<BufferResource> resource;
    };

    struct ImageResourceInfo : public ResourceInfo
    {
        std::shared_ptr<ImageResource> resource;
    };

    std::unordered_map<size_t, BufferResourceInfo> m_buffer_cache;
    std::unordered_map<size_t, ImageResourceInfo> m_image_cache;
};

} // namespace Mizu