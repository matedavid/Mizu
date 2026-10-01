#pragma once

#include <string>
#include <string_view>

#include "asset/asset_handle.h"
#include "render_core/rhi/shader.h"

#include "mizu_shader_module.h"
#include "shader/shader_compiler.h"
#include "shader/shader_declaration.h"

namespace Mizu
{

MIZU_SHADER_API std::string get_shader_declaration_virtual_path(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment);

MIZU_SHADER_API ShaderAssetHandle get_shader_declaration_asset_handle(const ShaderDeclaration& declaration);
MIZU_SHADER_API ShaderAssetHandle get_shader_declaration_asset_handle(const ShaderInstance& instance);
MIZU_SHADER_API ShaderAssetHandle get_shader_declaration_asset_handle(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment);

} // namespace Mizu