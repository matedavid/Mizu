#include "texture_cooker.h"

#include <stb_image.h>
#include <stb_image_resize2.h>

#include "asset/asset.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

//
// TextureImporter
//

std::span<const std::string_view> TextureImporter::extensions() const
{
    static constexpr std::string_view extensions[]{
        ".png",
        ".jpg",
    };

    return extensions;
}

uint32_t TextureImporter::version() const
{
    return 1;
}

bool TextureImporter::should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const
{
    const size_t id = get_texture_asset_id(request.virtual_path);
    return timestamp_should_import(id, request.path, version(), timestamp_db);
}

void TextureImporter::import(
    const ImportRequest& request,
    const CookContext& context,
    std::vector<CookRequest>& outputs)
{
    MIZU_ASSERT(std::filesystem::exists(request.path), "Texture path '{}' does not exist", request.path.string());

    const size_t id = get_texture_asset_id(request.virtual_path);
    timestamp_record(id, request.path, version(), context.timestamp_db);

    const std::string str_path = request.path.string();

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(str_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr)
    {
        context.reporter.error("Failed to read image file '{}'", str_path);
        return;
    }

    static constexpr bool GENERATE_MIPS = true;
    static constexpr uint32_t MAX_MIPS = 6;

    // TODO: Some information here should probably come from the asset metadata file
    TextureAssetMetadata metadata{};
    metadata.width = static_cast<uint32_t>(width);
    metadata.height = static_cast<uint32_t>(height);
    metadata.depth = 1;
    metadata.num_mips = GENERATE_MIPS ? std::min(compute_num_mips(metadata.width, metadata.height, 1), MAX_MIPS) : 1;
    metadata.format = ImageFormat::R8G8B8A8_UNORM; // TODO: Incorrect for albedo textures

    const uint64_t total_size = metadata.get_total_size_bytes();

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Texture");

    // First mip
    memcpy(data.data(), pixels, metadata.get_mip_size_bytes(0));

    // The rest of mips
    for (uint32_t mip = 1; mip < metadata.num_mips; ++mip)
    {
        const glm::uvec2 src_size = compute_mip_size(metadata.width, metadata.height, mip - 1);
        const glm::uvec2 dst_size = compute_mip_size(metadata.width, metadata.height, mip);

        const int32_t src_width = static_cast<int32_t>(src_size.x);
        const int32_t src_height = static_cast<int32_t>(src_size.y);

        const int32_t dst_width = static_cast<int32_t>(dst_size.x);
        const int32_t dst_height = static_cast<int32_t>(dst_size.y);

        uint8_t* src = data.data() + metadata.get_mip_offset(mip - 1);
        uint8_t* dst = data.data() + metadata.get_mip_offset(mip);

        if (is_srgb_format(metadata.format))
        {
            stbir_resize_uint8_srgb(src, src_width, src_height, 0, dst, dst_width, dst_height, 0, STBIR_RGBA);
        }
        else
        {
            stbir_resize_uint8_linear(src, src_width, src_height, 0, dst, dst_width, dst_height, 0, STBIR_RGBA);
        }
    }

    stbi_image_free(pixels);

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

//
// TextureCooker
//

bool TextureCooker::should_cook(const CookRequest&, const TimestampDb&) const
{
    // Filtering done by importer
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

    const TextureAssetMetadata& metadata = payload->metadata;
    const std::span<const uint8_t> pixels = payload->data;

    const size_t total_size = TOTAL_TEXTURE_METADATA_SIZE + metadata.get_total_size_bytes();

    std::span<uint8_t> data = context.allocator.allocate(total_size);
    MIZU_ASSERT(data.size() == total_size, "Failed to allocated data for Texture");

    texture_serialize_metadata(metadata, data);

    const size_t data_offset = TOTAL_TEXTURE_METADATA_SIZE;

    memcpy(data.data() + data_offset, pixels.data(), metadata.get_total_size_bytes());

    context.allocator.free(pixels);

    const std::string filename = std::to_string(get_texture_asset_id(request.virtual_path));
    outputs.push_back({
        .filename = filename,
        .data = data,
    });
}

} // namespace Mizu