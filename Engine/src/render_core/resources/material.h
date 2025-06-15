#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "render_core/resources/texture.h"

#include "render_core/shader/shader_group.h"
#include "render_core/shader/shader_properties.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

// Forward declarations
class Shader;
class BufferResource;
class ImageResourceView;
class SamplerState;
class ResourceGroup;

struct MaterialResourceGroup
{
    std::shared_ptr<ResourceGroup> resource_group;
    uint32_t set;
};

class Material
{
  public:
    Material(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader);

    void set(const std::string& name, std::shared_ptr<ImageResourceView> resource);
    void set(const std::string& name, std::shared_ptr<BufferResource> resource);
    void set(const std::string& name, std::shared_ptr<SamplerState> resource);

    bool bake();
    bool is_baked() const { return m_is_baked; }

    size_t get_hash() const;

    const std::vector<MaterialResourceGroup>& get_resource_groups() const { return m_resource_groups; }

    std::shared_ptr<Shader> get_vertex_shader() const { return m_vertex_shader; }
    std::shared_ptr<Shader> get_fragment_shader() const { return m_fragment_shader; }

  private:
    std::shared_ptr<Shader> m_vertex_shader{nullptr};
    std::shared_ptr<Shader> m_fragment_shader{nullptr};

    ShaderGroup m_shader_group;

    using MaterialDataT = std::
        variant<std::shared_ptr<ImageResourceView>, std::shared_ptr<BufferResource>, std::shared_ptr<SamplerState>>;
    struct MaterialData
    {
        ShaderProperty property;
        MaterialDataT value;
    };
    std::vector<MaterialData> m_resources;

    std::vector<MaterialResourceGroup> m_resource_groups;

    bool m_is_baked = false;
};

} // namespace Mizu
