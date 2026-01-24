#pragma once

#include <memory>
#include <string_view>
#include <variant>
#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "render_core/definitions/shader_types.h"
#include "render_core/rhi/resource_group.h"
#include "shader/shader_declaration.h"

#include "mizu_render_module.h"

namespace Mizu
{

// Forward declarations
class Shader;
class ImageResource;
class SamplerState;

struct MaterialResourceGroup
{
    std::shared_ptr<ResourceGroup> resource_group;
    uint32_t set;
};

struct MaterialShaderInstance
{
    std::string virtual_path;
    std::string_view entry_point;
};

class MIZU_RENDER_API Material
{
  public:
    Material(MaterialShaderInstance shader_instance);

    void set_texture_srv(const std::string& name, std::shared_ptr<ImageResource> resource);
    void set_sampler_state(const std::string& name, std::shared_ptr<SamplerState> resource);

    bool bake();
    bool is_baked() const { return m_is_baked; }

    const MaterialShaderInstance& get_shader_instance() const { return m_shader_instance; }

    size_t get_pipeline_hash() const { return m_pipeline_hash; }
    size_t get_material_hash() const { return m_material_hash; };

    const std::vector<MaterialResourceGroup>& get_resource_groups() const { return m_resource_groups; }

  private:
    MaterialShaderInstance m_shader_instance;
    const SlangReflection& m_shader_reflection;

    using MaterialResourceT = std::variant<std::shared_ptr<ImageResource>, std::shared_ptr<SamplerState>>;
    struct MaterialData
    {
        MaterialResourceT resource;
        ResourceGroupItem item;
        uint32_t set;
    };
    std::vector<MaterialData> m_resources;

    std::vector<MaterialResourceGroup> m_resource_groups;

    bool m_is_baked = false;
    size_t m_pipeline_hash = 0;
    size_t m_material_hash = 0;

    size_t get_pipeline_hash_internal() const;
    size_t get_material_hash_internal() const;
};

} // namespace Mizu
