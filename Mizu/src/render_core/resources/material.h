#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "render_core/resources/texture.h"
#include "render_core/shader/shader_properties.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

// Forward declarations
class GraphicsShader;
class BufferResource;
class ImageResourceView;
class SamplerState;
class ResourceGroup;

class Material
{
  public:
    Material(std::shared_ptr<GraphicsShader> shader);

    void set(const std::string& name, std::shared_ptr<ImageResourceView> resource);
    void set(const std::string& name, std::shared_ptr<BufferResource> resource);
    void set(const std::string& name, std::shared_ptr<SamplerState> resource);

    [[nodiscard]] bool bake();
    [[nodiscard]] bool is_baked() const { return m_is_baked; }

    [[nodiscard]] size_t get_hash() const;

    [[nodiscard]] std::vector<std::shared_ptr<ResourceGroup>> get_resource_groups() const { return m_resource_groups; }
    [[nodiscard]] std::shared_ptr<GraphicsShader> get_shader() const { return m_shader; }

  private:
    std::shared_ptr<GraphicsShader> m_shader;

    using MaterialDataT = std::
        variant<std::shared_ptr<ImageResourceView>, std::shared_ptr<BufferResource>, std::shared_ptr<SamplerState>>;
    struct MaterialData
    {
        ShaderProperty property;
        MaterialDataT value;
    };
    std::vector<MaterialData> m_resources;

    std::vector<std::shared_ptr<ResourceGroup>> m_resource_groups;

    bool m_is_baked = false;
};

} // namespace Mizu
