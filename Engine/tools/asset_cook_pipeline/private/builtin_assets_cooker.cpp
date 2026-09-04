#include "builtin_assets_cooker.h"

#include "asset/builtin_assets.h"

namespace Mizu
{

enum class BuiltinAssetType
{
    Texture,
};

struct BuiltinTextureInfo
{
    uint32_t width = 1, height = 1, depth = 1;
    ImageFormat format = ImageFormat::R8G8B8A8_UNORM;
    BuiltinTexture kind = BuiltinTexture::White;
};

struct BuiltinAssetImportPayload
{
    BuiltinAssetType type;

    union {
        BuiltinTextureInfo texture;
    };
};

static constexpr std::string_view get_builtin_asset_type_extension(BuiltinAssetType type)
{
    switch (type)
    {
    case BuiltinAssetType::Texture:
        return "builtin_texture";
    }
}

static std::span<const uint8_t> get_builtin_texture_data(BuiltinTexture texture)
{
    static constexpr uint8_t BUILTIN_TEXTURE_WHITE_DATA[] = {255, 255, 255, 255};
    static constexpr uint8_t BUILTIN_TEXTURE_BLACK_DATA[] = {0, 0, 0, 255};
    static constexpr uint8_t BUILTIN_TEXTURE_GRAY_DATA[] = {128, 128, 128, 255};

    switch (texture)
    {
    case BuiltinTexture::White:
        return BUILTIN_TEXTURE_WHITE_DATA;
    case BuiltinTexture::Black:
        return BUILTIN_TEXTURE_BLACK_DATA;
    case BuiltinTexture::Gray:
        return BUILTIN_TEXTURE_GRAY_DATA;
    }
}

//
// BuiltinAssetsRequestSource
//

static constexpr BuiltinAssetImportPayload BUILTIN_ASSETS[] = {
    {
        .type = BuiltinAssetType::Texture,
        .texture =
            {
                .width = 1,
                .height = 1,
                .kind = BuiltinTexture::White,
            },
    },
    {
        .type = BuiltinAssetType::Texture,
        .texture =
            {
                .width = 1,
                .height = 1,
                .kind = BuiltinTexture::Black,
            },
    },
    {
        .type = BuiltinAssetType::Texture,
        .texture =
            {
                .width = 1,
                .height = 1,
                .kind = BuiltinTexture::Gray,
            },
    },
};

bool BuiltinAssetsRequestSource::init(const CookContext&)
{
    m_cursor = 0;
    return true;
}

uint32_t BuiltinAssetsRequestSource::enumerate_n(uint32_t number, std::vector<ImportRequest>& outputs)
{
    uint32_t num_enumerated = 0;

    while (m_cursor < std::size(BUILTIN_ASSETS) && num_enumerated < number)
    {
        const BuiltinAssetImportPayload& payload = BUILTIN_ASSETS[m_cursor];

        outputs.push_back({
            .extension = std::string{get_builtin_asset_type_extension(payload.type)},
            .path = std::filesystem::path(),
            .virtual_path = std::string{get_builtin_texture_virtual_path(payload.texture.kind)},
            .asset_mount = AssetMount{.path = "", .name = "engine"},
            .payload = payload,
        });

        num_enumerated += 1;
        m_cursor += 1;
    }

    return num_enumerated;
}

//
// BuiltinAssetsImporter
//

std::span<const std::string_view> BuiltinAssetsImporter::extensions() const
{
    static constexpr std::string_view extensions[]{
        get_builtin_asset_type_extension(BuiltinAssetType::Texture),
    };

    return extensions;
}

uint32_t BuiltinAssetsImporter::version() const
{
    return 0;
}

bool BuiltinAssetsImporter::should_import(const ImportRequest&, const TimestampDb&) const
{
    // TODO:
    return true;
}

void BuiltinAssetsImporter::import(
    const ImportRequest& request,
    const CookContext& context,
    std::vector<CookRequest>& outputs)
{
    const BuiltinAssetImportPayload* payload = request.payload.get_if<BuiltinAssetImportPayload>();
    if (payload == nullptr)
    {
        MIZU_ASSERT(false, "Wrong payload type");
        return;
    }

    TextureAssetMetadata metadata{};
    metadata.width = static_cast<uint32_t>(payload->texture.width);
    metadata.height = static_cast<uint32_t>(payload->texture.height);
    metadata.depth = static_cast<uint32_t>(payload->texture.depth);
    metadata.num_mips = 1;
    metadata.format = payload->texture.format;

    const uint64_t total_size = metadata.get_total_size_bytes();

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Texture");

    const std::span<const uint8_t> pixels = get_builtin_texture_data(payload->texture.kind);
    MIZU_ASSERT(pixels.size() == total_size, "Builtin texture data size does not match metadata");

    memcpy(data.data(), pixels.data(), total_size);

    outputs.push_back({
        .asset_type = AssetType::Texture,
        .virtual_path = request.virtual_path,
        .asset_mount = request.asset_mount,
        .payload =
            TextureCookPayload{
                .data = data,
                .metadata = metadata,
            },
    });
}

} // namespace Mizu