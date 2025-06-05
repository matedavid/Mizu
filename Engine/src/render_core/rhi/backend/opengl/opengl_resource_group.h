#pragma once

#include <string>
#include <unordered_map>

#include "render_core/rhi/resource_group.h"

namespace Mizu::OpenGL
{

// Forward declarations
class OpenGLBufferResource;
class OpenGLImageResource;
class OpenGLShaderBase;

/*
class OpenGLResourceGroup : public ResourceGroup
{
  public:
    OpenGLResourceGroup() = default;
    ~OpenGLResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<ImageResourceView> image_view) override;
    void add_resource(std::string_view name, std::shared_ptr<BufferResource> buffer_resource) override;
    void add_resource(std::string_view name, std::shared_ptr<SamplerState> sampler_state) override;
    void add_resource(std::string_view name, std::shared_ptr<TopLevelAccelerationStructure> tlas) override;

    [[nodiscard]] size_t get_hash() const override;

    [[nodiscard]] bool bake(const IShader& shader, uint32_t set) override;
    [[nodiscard]] uint32_t currently_baked_set() const override { return 0; }

    void bind(const OpenGLShaderBase& shader) const;

  private:
    bool m_baked = false;

    std::unordered_map<std::string, std::shared_ptr<OpenGLImageResource>> m_image_resources;
    std::unordered_map<std::string, std::shared_ptr<OpenGLBufferResource>> m_ubo_resources;
};
*/

} // namespace Mizu::OpenGL