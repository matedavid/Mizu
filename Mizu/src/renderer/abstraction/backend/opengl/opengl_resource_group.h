#pragma once

#include <string>
#include <unordered_map>

#include "renderer/abstraction/resource_group.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLTexture2D;
class OpenGLUniformBuffer;

class OpenGLResourceGroup : public ResourceGroup {
  public:
    OpenGLResourceGroup() = default;
    ~OpenGLResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) override;
    void add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) override;

    [[nodiscard]] bool bake(const std::shared_ptr<IShader>& shader, uint32_t set) override;
    [[nodiscard]] uint32_t currently_baked_set() const override { return 0; }

  private:
    std::unordered_map<std::string, std::shared_ptr<OpenGLTexture2D>> m_texture_resources;
    std::unordered_map<std::string, std::shared_ptr<OpenGLUniformBuffer>> m_ubo_resources;
};

} // namespace Mizu::OpenGL