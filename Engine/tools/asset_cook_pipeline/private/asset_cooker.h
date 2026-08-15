#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "asset/asset.h"
#include "shader/shader_compiler.h"

// clang-format off
namespace Assimp { class Importer; }
struct aiMesh;
struct aiMaterial;
// clang-format on

namespace Mizu
{

enum class AssetCookType
{
    Mesh,
    Texture,
    Material,
    ShaderDeclaration,
};

struct MeshAssetCookInfo
{
    // Storing here to keep reference to the importer alive while the scene is being used
    std::shared_ptr<Assimp::Importer> importer;
    const aiMesh* mesh;
};

struct TextureAssetCookInfo
{
};

struct MaterialAssetCookInfo
{
    // Storing here to keep reference to the importer alive while the scene is being used
    std::shared_ptr<Assimp::Importer> importer;
    const aiMaterial* material;
};

struct ShaderDeclarationCookInfo
{
    std::filesystem::path path;
    ShaderBytecodeTarget bytecode_target;
    ShaderCompilationEnvironment environment{};
};

using AssetCookInfoT =
    std::variant<MeshAssetCookInfo, TextureAssetCookInfo, MaterialAssetCookInfo, ShaderDeclarationCookInfo>;

struct CookContext
{
    const AssetMountTable& asset_mounts;
};

struct ImportRequest
{
    std::filesystem::path physical_path;
    std::string_view virtual_path;

    const AssetMountTable& asset_mounts;
};

struct CookRequest
{
    AssetCookType asset_type;
    AssetCookInfoT cook_info;
};

class IAssetImporter
{
  public:
    virtual ~IAssetImporter() = default;

    virtual std::span<std::string_view> extensions() const = 0;

    virtual uint32_t import(const ImportRequest& input, std::vector<CookRequest>& outputs) const = 0;
};

class IAssetCooker
{
  public:
    virtual AssetCookType asset_type() const = 0;

    virtual bool cook(const AssetCookInfoT& cook_info) const = 0;
};

class ICookRequestSource
{
  public:
    virtual ~ICookRequestSource() = default;

    virtual void init(const CookContext& context) = 0;

    virtual uint32_t enumerate_n(const CookContext& context, uint32_t number, std::vector<CookRequest>& outputs) = 0;
};

} // namespace Mizu