#pragma once

#include "asset_cooker.h"

namespace Mizu
{

class BuiltinAssetsRequestSource : public IRequestSource
{
  public:
    bool init(const CookContext& context) override;
    uint32_t enumerate_n(uint32_t number, const CookContext& context, std::vector<ImportRequest>& outputs) override;

  private:
    uint32_t m_cursor = 0;
};

class BuiltinTextureImporter : public IAssetImporter
{
  public:
    std::span<const std::string_view> extensions() const override;
    uint32_t version() const override;

    bool should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const override;
    void import(const ImportRequest& request, const CookContext& context, std::vector<CookRequest>& outputs) override;
};

} // namespace Mizu