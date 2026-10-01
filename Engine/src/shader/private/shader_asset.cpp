#include "shader/shader_asset.h"

#include <format>

#include "asset/asset.h"

namespace Mizu
{

namespace
{

std::string_view get_shader_type_suffix(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Vertex:
        return "vs";
    case ShaderType::Fragment:
        return "fs";
    case ShaderType::Compute:
        return "cs";
    case ShaderType::RtxRaygen:
        return "raygen";
    case ShaderType::RtxClosestHit:
        return "closesthit";
    case ShaderType::RtxMiss:
        return "miss";
    case ShaderType::RtxIntersection:
        return "intersection";
    case ShaderType::RtxAnyHit:
        return "anyhit";
    }
}

} // namespace

std::string get_shader_declaration_virtual_path(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    return std::format(
        "{}_{}_{}{}",
        virtual_path,
        entry_point,
        get_shader_type_suffix(type),
        environment.get_shader_filename_string());
}

ShaderAssetHandle get_shader_declaration_asset_handle(const ShaderDeclaration& declaration)
{
    return get_shader_declaration_asset_handle(declaration.get_instance());
}

ShaderAssetHandle get_shader_declaration_asset_handle(const ShaderInstance& instance)
{
    return get_shader_declaration_asset_handle(
        instance.virtual_path, instance.entry_point, instance.type, instance.environment);
}

ShaderAssetHandle get_shader_declaration_asset_handle(
    std::string_view virtual_path,
    std::string_view entry_point,
    ShaderType type,
    const ShaderCompilationEnvironment& environment)
{
    return get_shader_declaration_asset_id(
        get_shader_declaration_virtual_path(virtual_path, entry_point, type, environment));
}

} // namespace Mizu