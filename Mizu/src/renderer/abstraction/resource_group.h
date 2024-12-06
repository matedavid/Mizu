#pragma once

#include <memory>
#include <string_view>

namespace Mizu {

// Forward declarations
class BufferResource;
class ImageResource;
class IShader;

class ResourceGroup {
  public:
    virtual ~ResourceGroup() = default;

    [[nodiscard]] static std::shared_ptr<ResourceGroup> create();

    virtual void add_resource(std::string_view name, std::shared_ptr<ImageResource> image_resource) = 0;
    virtual void add_resource(std::string_view name, std::shared_ptr<BufferResource> buffer_resource) = 0;

    [[nodiscard]] virtual bool bake(const std::shared_ptr<IShader>& shader, uint32_t set) = 0;
    [[nodiscard]] virtual uint32_t currently_baked_set() const = 0;
};

} // namespace Mizu
