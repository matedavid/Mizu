#pragma once

#include <memory>
#include <variant>
#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "renderer/render_graph_renderer_shaders.h"
#include "renderer/shader/shader_declaration.h"
#include "renderer/shader/shader_types.h"

#include "render_core/resources/texture.h"

#include "render_core/shader/shader_group.h"
#include "render_core/shader/shader_properties.h"

namespace Mizu
{

// Forward declarations
class Shader;
class BufferResource;
class ImageResourceView;
class SamplerState;
class ResourceGroup;

class PBROpaqueShaderVS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/PBROpaque.slang", ShaderType::Vertex, "vsMain");
};

class PBROpaqueShaderFS : public ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER_DECLARATION("/EngineShaders/forwardplus/PBROpaque.slang", ShaderType::Fragment, "fsMain");

    static void modify_compilation_environment(
        [[maybe_unused]] const ShaderCompilationTarget& target,
        ShaderCompilationEnvironment& environment)
    {
        environment.set_define("TILE_SIZE", LightCullingShaderCS::TILE_SIZE);
        environment.set_define("MAX_LIGHTS_PER_TILE", LightCullingShaderCS::MAX_LIGHTS_PER_TILE);
    }
};

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

    size_t get_pipeline_hash() const { return m_pipeline_hash; }
    size_t get_material_hash() const { return m_material_hash; };

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
        ShaderResource resource;
        MaterialDataT value;
    };
    std::vector<MaterialData> m_resources;

    std::vector<MaterialResourceGroup> m_resource_groups;

    bool m_is_baked = false;
    size_t m_pipeline_hash = 0;
    size_t m_material_hash = 0;

    size_t compute_pipeline_hash() const;
    size_t compute_material_hash() const;
};

} // namespace Mizu
