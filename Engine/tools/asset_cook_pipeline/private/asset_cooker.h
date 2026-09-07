#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "asset/asset.h"
#include "asset/asset_handle.h"
#include "base/containers/inplace_any.h"

#include "asset_cook_reporter.h"
#include "free_range_allocator.h"
#include "timestamp_db.h"

namespace Mizu
{

struct CookContext
{
    const AssetMountTable& asset_mounts;
    TimestampDb& timestamp_db;
    FreeRangeAllocator& allocator;
    AssetCookReporter& reporter;
};

static constexpr size_t ASSET_PAYLOAD_SIZE = 128;
using AssetPayload = inplace_any<ASSET_PAYLOAD_SIZE>;

struct ImportRequest
{
    std::string extension;
    std::filesystem::path path;
    std::string virtual_path;
    AssetMount asset_mount;
    AssetPayload payload;
};

struct CookRequest
{
    AssetType asset_type;
    std::string virtual_path;
    AssetMount asset_mount;
    AssetPayload payload;
};

struct SinkRequest
{
    std::string filename;
    std::span<uint8_t> data;
};

class IRequestSource
{
  public:
    virtual ~IRequestSource() = default;

    virtual bool init(const CookContext& context) = 0;

    virtual uint32_t enumerate_n(uint32_t number, const CookContext& context, std::vector<ImportRequest>& outputs) = 0;
};

class IAssetImporter
{
  public:
    virtual ~IAssetImporter() = default;

    virtual std::span<const std::string_view> extensions() const = 0;
    virtual uint32_t version() const = 0;

    virtual bool should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const = 0;
    virtual void import(
        const ImportRequest& request,
        const CookContext& context,
        std::vector<CookRequest>& outputs) = 0;
};

class IAssetCooker
{
  public:
    virtual ~IAssetCooker() = default;

    virtual AssetType asset_type() const = 0;

    virtual bool should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const = 0;
    virtual void cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs) = 0;
};

} // namespace Mizu
