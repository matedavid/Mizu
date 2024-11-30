#pragma once

#include <cstdint>
#include <memory>

#include "core/uuid.h"

namespace Mizu {

// Forward declarations
class ImageResource;

using Allocation = UUID;

class IDeviceMemoryAllocator {
  public:
    virtual ~IDeviceMemoryAllocator() = default;

    virtual Allocation allocate_image_resource(const ImageResource& image) = 0;

    virtual void release(Allocation id) = 0;
};

class BaseDeviceMemoryAllocator : public IDeviceMemoryAllocator {
  public:
    static std::shared_ptr<BaseDeviceMemoryAllocator> create();
};

//
// RenderGraphDeviceMemoryAllocator stuff
//

struct ImageDescription;
struct SamplingOptions;

class TransientImageResource {
  public:
    static std::shared_ptr<TransientImageResource> create(const ImageDescription& desc,
                                                          const SamplingOptions& sampling);

    [[nodiscard]] virtual size_t get_size() const = 0;

    [[nodiscard]] virtual std::shared_ptr<ImageResource> get_resource() const = 0;
};

class RenderGraphDeviceMemoryAllocator {
  public:
    virtual ~RenderGraphDeviceMemoryAllocator() = default;

    static std::shared_ptr<RenderGraphDeviceMemoryAllocator> create();

    virtual void allocate_image_resource(const TransientImageResource& resource, size_t offset) = 0;

    virtual void allocate() = 0;

    virtual size_t get_size() const = 0;
};

} // namespace Mizu