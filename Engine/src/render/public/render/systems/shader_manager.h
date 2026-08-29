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

    void add_shader_mapping(std::string_view mapping, std::filesystem::path path);
    void remove_shader_mapping(std::string_view mapping);

    std::shared_ptr<Shader> get_shader(
        std::string_view virtual_path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment);

    const SlangReflection* get_reflection(
        std::string_view virtual_path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment);

    static size_t get_shader_hash(
        std::string_view virtual_path,
        std::string_view entry_point,
        ShaderType type,
        const ShaderCompilationEnvironment& environment);

  private:
    std::unordered_map<std::string, std::filesystem::path> m_path_mappings;

    std::unordered_map<ShaderDeclarationAssetHandle, std::shared_ptr<Shader>> m_shader_cache{};
    std::unordered_map<ShaderDeclarationAssetHandle, SlangReflection> m_reflection_cache{};

    bool load_shader_and_reflection(ShaderDeclarationAssetHandle handle, std::string_view entry_point, ShaderType type);
};

} // namespace Mizu