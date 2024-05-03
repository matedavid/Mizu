#pragma once

#include <memory>
#include <string_view>

namespace Mizu {

// Forward declarations
class Texture2D;
class UniformBuffer;

class ResourceGroup {
  public:
    virtual ~ResourceGroup() = default;

    [[nodiscard]] static std::shared_ptr<ResourceGroup> create();

    virtual void add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) = 0;
    virtual void add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) = 0;
};

} // namespace Mizu
