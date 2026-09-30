#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "asset/asset_handle.h"
#include "render_core/rhi/shader.h"
#include "shader/shader_compiler.h"

#include "mizu_render_module.h"

namespace Mizu
{

// Forward declarations
enum class GraphicsApi;

class MIZU_RENDER_API ShaderManager
{
  public:
    static ShaderManager& get();
    void reset();

    std::shared_ptr<Shader> get_shader(ShaderAssetHandle handle);
    const SlangReflection* get_reflection(ShaderAssetHandle handle);

  private:
    std::unordered_map<ShaderAssetHandle, std::shared_ptr<Shader>> m_shader_cache{};
    std::unordered_map<ShaderAssetHandle, SlangReflection> m_reflection_cache{};

    bool load_shader_and_reflection(ShaderAssetHandle handle, ShaderBytecodeTarget target);
};

MIZU_RENDER_API ShaderAssetHandle get_shader_declaration_asset_handle(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment);

MIZU_RENDER_API std::shared_ptr<Shader> get_shader(const ShaderDeclaration& declaration);
MIZU_RENDER_API std::shared_ptr<Shader> get_shader(const ShaderInstance& instance);
MIZU_RENDER_API std::shared_ptr<Shader> get_shader(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment);
MIZU_RENDER_API std::shared_ptr<Shader> get_shader(ShaderAssetHandle handle);

MIZU_RENDER_API const SlangReflection* get_shader_reflection(const ShaderDeclaration& declaration);
MIZU_RENDER_API const SlangReflection* get_shader_reflection(const ShaderInstance& instance);
MIZU_RENDER_API const SlangReflection* get_shader_reflection(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment);
MIZU_RENDER_API const SlangReflection* get_shader_reflection(ShaderAssetHandle handle);

} // namespace Mizu