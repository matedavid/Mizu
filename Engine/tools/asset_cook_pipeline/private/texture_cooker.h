#pragma once

#include "asset/asset_metadata.h"

#include "asset_cooker.h"

namespace Mizu
{

struct TextureCookPayload
{
    std::filesystem::path path;
    TextureMetadata metadata{};
};

class TextureImporter : public IAssetImporter
{
  public:
    std::span<const std::string_view> extensions() const override;
    uint32_t version() const override;

    bool should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const override;
    void import(const ImportRequest& request, const CookContext& context, std::vector<CookRequest>& outputs) override;
};

class TextureCooker : public IAssetCooker
{
  public:
    AssetType asset_type() const override { return AssetType::Texture; }

    bool should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const override;
    void cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs) override;
};

} // namespace Mizu