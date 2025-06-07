#pragma once

#include <functional>
#include <memory>
#include <variant>

#include "managers/shader_manager.h"
#include "render_core/render_graph/render_graph_types.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/shader.h"
#include "render_core/shader/shader_parameters.h"

namespace Mizu
{

#define IMPLEMENT_GRAPHICS_SHADER(vert_path, vert_entry_point, frag_path, frag_entry_point)                   \
    static std::shared_ptr<Mizu::IShader> get_shader()                                                        \
    {                                                                                                         \
        return Mizu::ShaderManager::get_shader({vert_path, vert_entry_point}, {frag_path, frag_entry_point}); \
    }

#define IMPLEMENT_COMPUTE_SHADER(comp_path, comp_entry_point)                  \
    static std::shared_ptr<Mizu::IShader> get_shader()                         \
    {                                                                          \
        return Mizu::ShaderManager::get_shader({comp_path, comp_entry_point}); \
    }

#define IMPLEMENT_SHADER(_path, _entry_point, _type)                       \
    static std::shared_ptr<Mizu::Shader> get_shader2()                     \
    {                                                                      \
        return Mizu::ShaderManager::get_shader2(Mizu::Shader::Description{ \
            .path = _path,                                                 \
            .entry_point = _entry_point,                                   \
            .type = _type,                                                 \
        });                                                                \
    }

class ShaderDeclaration
{
  public:
    static std::shared_ptr<IShader> get_shader() { return nullptr; }
    static std::shared_ptr<Shader> get_shader2() { return nullptr; }
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

} // namespace Mizu
