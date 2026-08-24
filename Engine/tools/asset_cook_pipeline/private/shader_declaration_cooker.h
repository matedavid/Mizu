#pragma once

#include <filesystem>

#include "shader/shader_compiler.h"
#include "shader/shader_registry.h"

#include "asset_cooker.h"

namespace Mizu
{

struct ShaderDeclarationImportPayload
{
    ShaderDeclarationMetadata metadata{};
};

struct ShaderDeclarationCookPayload
{
    std::filesystem::path path{};
    ShaderBytecodeTarget bytecode_target{};
    ShaderCompilationEnvironment environment{};
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
    void cook(const CookRequest& request, std::vector<SinkRequest>& outputs) override;
};

} // namespace Mizu