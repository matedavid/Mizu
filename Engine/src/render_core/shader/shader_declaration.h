#pragma once

#include <functional>
#include <memory>
#include <variant>

#include "managers/shader_manager.h"

#include "render_core/render_graph/render_graph_types.h"

#include "render_core/shader/shader_parameters.h"

#include "render_core/rhi/shader.h"

#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/rtx/ray_tracing_pipeline.h"

namespace Mizu
{

#define IMPLEMENT_GRAPHICS_SHADER_DECLARATION(_vert_path, _vert_entry_point, _frag_path, _frag_entry_point) \
    ShaderDescription get_shader_description() const override                                               \
    {                                                                                                       \
        Mizu::Shader::Description vs_desc{};                                                                \
        vs_desc.path = _vert_path;                                                                          \
        vs_desc.entry_point = _vert_entry_point;                                                            \
        vs_desc.type = Mizu::ShaderType::Vertex;                                                            \
                                                                                                            \
        Mizu::Shader::Description fs_desc{};                                                                \
        fs_desc.path = _frag_path;                                                                          \
        fs_desc.entry_point = _frag_entry_point;                                                            \
        fs_desc.type = Mizu::ShaderType::Fragment;                                                          \
                                                                                                            \
        ShaderDescription desc{};                                                                           \
        desc.vertex = Mizu::ShaderManager::get_shader(vs_desc);                                             \
        desc.fragment = Mizu::ShaderManager::get_shader(fs_desc);                                           \
                                                                                                            \
        return desc;                                                                                        \
    }

#define IMPLEMENT_COMPUTE_SHADER_DECLARATION(_comp_path, _comp_entry_point) \
    ShaderDescription get_shader_description() const override               \
    {                                                                       \
        Mizu::Shader::Description cs_desc{};                                \
        cs_desc.path = _comp_path;                                          \
        cs_desc.entry_point = _comp_entry_point;                            \
        cs_desc.type = Mizu::ShaderType::Compute;                           \
                                                                            \
        ShaderDescription desc{};                                           \
        desc.compute = Mizu::ShaderManager::get_shader(cs_desc);            \
                                                                            \
        return desc;                                                        \
    }

class ShaderDeclaration
{
};

class GraphicsShaderDeclaration : public ShaderDeclaration
{
  public:
    struct ShaderDescription
    {
        std::shared_ptr<Shader> vertex;
        std::shared_ptr<Shader> fragment;
    };

    static GraphicsPipeline::Description get_pipeline_template(const ShaderDescription& desc)
    {
        GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.vertex_shader = desc.vertex;
        pipeline_desc.fragment_shader = desc.fragment;

        return pipeline_desc;
    }

    virtual ShaderDescription get_shader_description() const = 0;
};

class ComputeShaderDeclaration : public ShaderDeclaration
{
  public:
    struct ShaderDescription
    {
        std::shared_ptr<Shader> compute;
    };

    static ComputePipeline::Description get_pipeline_template(const ShaderDescription& desc)
    {
        ComputePipeline::Description pipeline_desc{};
        pipeline_desc.shader = desc.compute;

        return pipeline_desc;
    }

    virtual ShaderDescription get_shader_description() const = 0;
};

class RayTracingShaderDeclaration : public ShaderDeclaration
{
  public:
    struct ShaderDescription
    {
        std::shared_ptr<Shader> raygen;
        std::shared_ptr<Shader> miss;
        std::shared_ptr<Shader> closest_hit;
    };

    static RayTracingPipeline::Description get_pipeline_template(const ShaderDescription& desc)
    {
        RayTracingPipeline::Description pipeline_desc{};
        pipeline_desc.raygen_shader = desc.raygen;
        pipeline_desc.miss_shader = desc.miss;
        pipeline_desc.closest_hit_shader = desc.closest_hit;

        return pipeline_desc;
    }

    virtual ShaderDescription get_shader_description() const = 0;
};

} // namespace Mizu
