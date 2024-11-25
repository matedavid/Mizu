#pragma once

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

} // namespace Mizu