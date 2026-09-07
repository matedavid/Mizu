#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "shader/shader_compiler.h"
#include "shader/shader_registry.h"

#include "asset_cooker.h"

namespace Mizu
{

struct ShaderDeclarationImportPayload
{
    ShaderDeclarationMetadata metadata{};
    std::vector<std::string> include_paths{};
};

struct ShaderDeclarationCookPayload
{
    std::filesystem::path path{};
    std::string_view entry_point{};
    ShaderType shader_type{};
    ShaderBytecodeTarget bytecode_target{};
    ShaderCompilationEnvironment environment{};
    std::vector<std::string> include_paths{};
};

class ShaderDeclarationRequestSource : public IRequestSource
{
  public:
    bool init(const CookContext& context) override;

    uint32_t enumerate_n(uint32_t number, const CookContext& context, std::vector<ImportRequest>& outputs) override;

  private:
    ShaderRegistry m_registry{};
    std::span<const ShaderDeclarationMetadata> m_shader_metadata{};
    uint32_t m_shader_cursor = 0;

    std::vector<std::string> m_include_paths{};
};

class ShaderDeclarationImporter : public IAssetImporter
{
  public:
    std::span<const std::string_view> extensions() const override;
    uint32_t version() const override;

    bool should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const override;
    void import(const ImportRequest& request, const CookContext& context, std::vector<CookRequest>& outputs) override;
};

class ShaderDeclarationCooker : public IAssetCooker
{
  public:
    AssetType asset_type() const override { return AssetType::ShaderDeclaration; }

    bool should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const override;
    void cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs) override;
};

} // namespace Mizu