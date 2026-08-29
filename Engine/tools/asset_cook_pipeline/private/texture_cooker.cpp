#include "texture_cooker.h"

#include <stb_image.h>

#include "asset/asset.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

//
// TextureImporter
//

std::span<const std::string_view> TextureImporter::extensions() const
{
    static std::string_view extensions[]{
        ".png",
        ".jpg",
    };

    return extensions;
}

uint32_t TextureImporter::version() const
{
    return 0;
}

bool TextureImporter::should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const
{
    const size_t id = hash_compute(request.virtual_path);

    const uint64_t last_write_time =
        static_cast<uint64_t>(std::filesystem::last_write_time(request.path).time_since_epoch().count());

    const Timestamp ts{
        .ts = last_write_time,
        .version = version(),
    };

    return timestamp_db.is_different(id, ts);
}

void TextureImporter::import(const ImportRequest& request, const CookContext&, std::vector<CookRequest>& outputs)
{
    // Just send through

    outputs.push_back({
        .asset_type = AssetType::Texture,
        .virtual_path = request.virtual_path,
        .payload =
            TextureCookPayload{
                .path = request.path,
                .metadata = TextureAssetMetadata{},
            },
    });
}

//
// TextureCooker
//

bool TextureCooker::should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const
{
    (void)request;
    (void)timestamp_db;

    return true;
}

void TextureCooker::cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs)
{
    const TextureCookPayload* payload = request.payload.get_if<TextureCookPayload>();
    if (payload == nullptr)
    {
        MIZU_ASSERT(false, "Wrong payload type");
        return;
    }

    MIZU_ASSERT(std::filesystem::exists(payload->path), "Texture path '{}' does not exist", payload->path.string());

    const std::string str_path = payload->path.string();

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(str_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr)
    {
        MIZU_LOG_ERROR("Failed to read image file '{}'", str_path);
        return;
    }

    // TODO: Some information here should probably come from the payload metadata

    TextureAssetMetadata metadata{};
    metadata.width = static_cast<uint32_t>(width);
    metadata.height = static_cast<uint32_t>(height);
    metadata.depth = 1;
    metadata.num_mips = 1;
    metadata.format = ImageFormat::R8G8B8A8_UNORM;

    const size_t total_size = TOTAL_TEXTURE_METADATA_SIZE + metadata.get_total_size_bytes();

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Texture");

    texture_serialize_metadata(metadata, data);

    const size_t data_offset = TOTAL_TEXTURE_METADATA_SIZE;

    memcpy(data.data() + data_offset, pixels, metadata.get_total_size_bytes());

    const std::string filename = std::to_string(get_texture_asset_id(request.virtual_path));
    outputs.push_back({
        .filename = filename,
        .data = data,
    });

    stbi_image_free(pixels);
}

} // namespace Mizu