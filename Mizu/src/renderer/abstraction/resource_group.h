#pragma once

#include <memory>
#include <string_view>

namespace Mizu {

// Forward declarations
class Texture2D;
class Cubemap;
class UniformBuffer;
class IShader;

class ResourceGroup {
  public:
    virtual ~ResourceGroup() = default;

    [[nodiscard]] static std::shared_ptr<ResourceGroup> create();

    virtual void add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) = 0;
    virtual void add_resource(std::string_view name, std::shared_ptr<Cubemap> cubemap) = 0;
    virtual void add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) = 0;

    [[nodiscard]] virtual bool bake(const std::shared_ptr<IShader>& shader, uint32_t set) = 0;
    [[nodiscard]] virtual uint32_t currently_baked_set() const = 0;
};

} // namespace Mizu
