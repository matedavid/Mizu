#pragma once

#include <functional>
#include <memory>
#include <variant>

#include "managers/shader_manager.h"
#include "render_core/render_graph/render_graph_types.h"
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

} // namespace Mizu
