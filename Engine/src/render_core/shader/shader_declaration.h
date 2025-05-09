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

class ShaderDeclaration
{
  public:
    static std::shared_ptr<IShader> get_shader() { return nullptr; }
};

} // namespace Mizu