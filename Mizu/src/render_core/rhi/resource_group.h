#pragma once

#include <memory>
#include <string_view>

namespace Mizu
{

// Forward declarations
class BufferResource;
class ImageResourceView;
class SamplerState;
class IShader;

class ResourceGroup
{
  public:
    virtual ~ResourceGroup() = default;

    [[nodiscard]] static std::shared_ptr<ResourceGroup> create();

    virtual void add_resource(std::string_view name, std::shared_ptr<ImageResourceView> image_view) = 0;
    virtual void add_resource(std::string_view name, std::shared_ptr<BufferResource> buffer_resource) = 0;
    virtual void add_resource(std::string_view name, std::shared_ptr<SamplerState> sampler_state) = 0;

    [[nodiscard]] virtual size_t get_hash() const = 0;

    [[nodiscard]] virtual bool bake(const IShader& shader, uint32_t set) = 0;
    [[nodiscard]] virtual uint32_t currently_baked_set() const = 0;
};

} // namespace Mizu
