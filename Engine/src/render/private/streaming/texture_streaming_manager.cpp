#include "streaming/texture_streaming_manager.h"

#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/math/math.h"
#include "registries/render_view_registry.h"
#include "registries/transform_registry.h"

namespace Mizu
{

namespace
{

glm::vec3 axis_scales(const glm::mat4& mat)
{
    return glm::vec3{
        glm::length(glm::vec3(mat[0])),
        glm::length(glm::vec3(mat[1])),
        glm::length(glm::vec3(mat[2])),
    };
}

float max_axis_scale(const glm::mat4& mat)
{
    const glm::vec3 s = axis_scales(mat);
    return glm::max(s.x, glm::max(s.y, s.z));
}

struct Sphere
{
    glm::vec3 center;
    float radius;
};

Sphere transform_sphere(const math::AABB& local, const glm::mat4& mat)
{
    const glm::vec3 local_center = 0.5f * (local.min() + local.max());
    const glm::vec3 local_half = 0.5f * (local.max() - local.min());

    const float radius = glm::length(local_half * axis_scales(mat));

    return Sphere{
        .center = glm::vec3(mat * glm::vec4(local_center, 1.0f)),
        .radius = radius,
    };
}

glm::vec2 viewport_pixel_size(const ViewportRect& viewport, glm::uvec2 output_size)
{
    return {
        std::round(viewport.extent.x * static_cast<float>(output_size.x)),
        std::round(viewport.extent.y * static_cast<float>(output_size.y)),
    };
}

float pixels_per_world_unit(const ViewportRect& viewport, float fov, float depth, glm::uvec2 viewport_size)
{
    const float height = viewport_pixel_size(viewport, viewport_size).y;
    const float tan_half_fov_y = glm::tan(fov / 2.0f);

    return height / (2.0f * depth * tan_half_fov_y);
}

float projected_area_fraction(const Sphere& sphere, const RenderViewRegistryEntry& view, glm::uvec2 viewport_size)
{
    const glm::vec2 viewport_px = viewport_pixel_size(view.viewport, viewport_size);
    const float screen_px = viewport_px.x * viewport_px.y;
    if (screen_px <= 0.0f)
        return 0.0f;

    if (glm::distance(sphere.center, view.camera.position) <= sphere.radius)
        return 1.0f;

    const float center_depth = -(view.view_matrix * glm::vec4(sphere.center, 1.0f)).z;

    if (center_depth + sphere.radius <= view.camera.znear)
        return 0.0f;

    const float depth = std::max(center_depth, view.camera.znear);

    const float radius_px = sphere.radius * pixels_per_world_unit(view.viewport, view.camera.fov, depth, viewport_size);
    const float area_px = glm::pi<float>() * radius_px * radius_px;

    return glm::clamp(area_px / screen_px, 0.0f, 1.0f);
}

constexpr uint32_t FRAMES_TO_UPDATE = 3u;

} // namespace

TextureStreamingManager::TextureStreamingManager(IAssetLoader& asset_loader) : m_asset_loader(asset_loader) {}

void TextureStreamingManager::update(const StreamingContext& context)
{
    if (m_update_cursor >= FRAMES_TO_UPDATE)
    {
        m_update_cursor = 0;
    }

    if (m_update_cursor == 0)
    {
        m_iterations_per_update = math::ceil_div(static_cast<uint32_t>(m_streamable_textures.size()), FRAMES_TO_UPDATE);
    }

    for (TextureAssetHandle handle : m_dirty_handles)
    {
        refresh(handle, context);
    }

    m_dirty_handles.clear();

    for (uint32_t i = 0; i < m_iterations_per_update; ++i)
    {
        const uint32_t streamable_idx = m_update_cursor * m_iterations_per_update + i;
        if (streamable_idx >= m_streamable_textures.size())
            break;

        const TextureAssetHandle handle = m_streamable_textures[streamable_idx];
        refresh(handle, context);
    }

    m_update_cursor += 1;
}

void TextureStreamingManager::gather(std::vector<StreamingRequest>& requests)
{
    for (const StreamingRequest& pending : m_pending_requests)
    {
        requests.push_back(pending);
    }

    m_pending_requests.clear();
}

void TextureStreamingManager::execute(std::span<StreamingAward> awards)
{
    (void)awards;
}

void TextureStreamingManager::record_gpu_uploads(CommandBuffer& command)
{
    (void)command;
}

void TextureStreamingManager::add_usage(const TextureStreamingUsage& usage)
{
    const auto it = m_usages_map.find(usage.streaming_handle);
    if (it != m_usages_map.end())
    {
        MIZU_UNREACHABLE(
            "Trying to add usage with streaming handle ({}) that already exists", usage.streaming_handle.id);
        return;
    }

    if (!m_residency.increment_reference_count(usage.texture_handle))
    {
        MIZU_UNREACHABLE("Failed to increment reference count");
        return;
    }

    m_usages_map.emplace(usage.streaming_handle, usage);

    auto texture_to_usages_it = m_texture_to_usages_map.find(usage.texture_handle);
    if (texture_to_usages_it == m_texture_to_usages_map.end())
    {
        texture_to_usages_it = m_texture_to_usages_map.insert({usage.texture_handle, {}}).first;

        if (is_streamable(usage.texture_handle))
        {
            m_streamable_textures.push_back(usage.texture_handle);
        }
    }

    texture_to_usages_it->second.push_back(usage);

    m_dirty_handles.push_back(usage.texture_handle);
}

void TextureStreamingManager::remove_usage(TextureStreamingHandle streaming_handle)
{
    const auto it = m_usages_map.find(streaming_handle);
    if (it == m_usages_map.end())
    {
        MIZU_UNREACHABLE("Trying to remove usage with owner handle ({}) that does not exist", streaming_handle.id);
        return;
    }

    const TextureStreamingUsage& usage = it->second;
    if (m_residency.decrement_reference_count(usage.texture_handle))
    {
        MIZU_UNREACHABLE("Failed to decrement reference count");
        return;
    }

    const auto texture_to_usages_it = m_texture_to_usages_map.find(usage.texture_handle);
    MIZU_ASSERT(texture_to_usages_it != m_texture_to_usages_map.end(), "Failed to find texture to usage entry");

    std::erase_if(texture_to_usages_it->second, [&usage](const TextureStreamingUsage& u) {
        return u.streaming_handle == usage.streaming_handle;
    });

    if (texture_to_usages_it->second.empty())
    {
        if (is_streamable(usage.texture_handle))
        {
            std::erase_if(m_streamable_textures, [&usage](const TextureAssetHandle& handle) {
                return handle == usage.texture_handle;
            });
        }

        m_texture_to_usages_map.erase(texture_to_usages_it);
    }

    m_dirty_handles.push_back(usage.texture_handle);
}

void TextureStreamingManager::refresh(TextureAssetHandle handle, const StreamingContext& context)
{
    const uint32_t references = m_residency.get_references(handle);
    const TextureResidencyPayload* payload = m_residency.get_payload(handle);
    MIZU_ASSERT(payload != nullptr, "Failed to get payload for texture handle: {}", handle.get_id());

    const std::optional<EvaluateResult> result = evaluate(handle, context, *payload, references);
    if (!result.has_value())
        return;

    if (result->desired_level != payload->resident_level)
    {
        const int8_t delta = static_cast<int8_t>(payload->resident_level) - static_cast<int8_t>(payload->desired_level);
        m_pending_requests.push_back({
            .manager = this,
            .asset_key = to_asset_key(handle),
            .resident_level = payload->resident_level,
            .desired_level = payload->desired_level,
            .level_delta = delta,
            .score = result->score,
        });
    }
}

auto TextureStreamingManager::evaluate(
    TextureAssetHandle handle,
    const StreamingContext& context,
    const TextureResidencyPayload& payload,
    uint32_t references) -> std::optional<EvaluateResult>
{
    constexpr float EVICTION_SCORE = 10.0f;
    constexpr float FIRST_LOAD_SCORE = 100.0f;

    // If it has no references, means that it has no further usages. Request eviction.
    if (references == 0)
    {
        return EvaluateResult{
            .desired_level = 0,
            .score = EVICTION_SCORE,
        };
    }

    const bool streamable = is_streamable(handle);

    // If it's not streamable, and the resident level is 0, it means it's the first load of the asset, and because
    // we don't have to take into account which quality to load, just request the load.
    if (!streamable && payload.resident_level == 0)
    {
        return EvaluateResult{
            .desired_level = 1,
            .score = FIRST_LOAD_SCORE,
        };
    }

    if (!streamable)
        return std::nullopt;

    const std::optional<TextureAssetRecord> record = m_asset_loader.get_texture_record(handle);
    MIZU_ASSERT(record.has_value(), "Failed to get texture record for streaming handle: {}", handle.get_id());

    const TextureAssetMetadata& metadata = record->metadata;

    const uint32_t texture_dim = glm::max(metadata.width, metadata.height);

    const int32_t mip_tail_start =
        static_cast<int32_t>(glm::min<uint32_t>(metadata.streamable_mips, metadata.num_mips - 1));
    // TODO: Kept as a variable in case in the future we add the ability to "skip mips"
    const int32_t finest_top_mip = 0;

    const TransformRegistry& transform_registry = transform_registry_get();

    const auto usages_it = m_texture_to_usages_map.find(handle);
    if (usages_it == m_texture_to_usages_map.end())
    {
        MIZU_UNREACHABLE("Failed to find usages for texture handle: {}", handle.get_id());
        return std::nullopt;
    }

    const std::vector<TextureStreamingUsage>& usages = usages_it->second;

    uint8_t desired_level = 0;
    float score = 0.0f;

    for (const TextureStreamingUsage& usage : usages)
    {
        const TransformInfo& transform_info = transform_registry.get_transform_info(usage.transform_handle);

        const float scale = max_axis_scale(transform_info.transform);
        const Sphere sphere = transform_sphere(usage.aabb, transform_info.transform);

        for (const RenderViewRegistryEntry& view : context.views)
        {
            const float center_depth = -(view.view_matrix * glm::vec4(sphere.center, 1.0f)).z;
            const float depth = glm::max(center_depth - sphere.radius, view.camera.znear);

            const float pixels_per_uv =
                usage.texel_factor * scale
                * pixels_per_world_unit(view.viewport, view.camera.fov, depth, context.viewport_size);
            const float texels_per_pixel = static_cast<float>(texture_dim) / std::max(pixels_per_uv, 0.0001f);

            constexpr float MIPS_BIAS = 0.5f;
            const float lod = std::log2(texels_per_pixel) - MIPS_BIAS;
            if (std::isnan(lod))
                continue;

            const float clamped =
                glm::clamp(lod, static_cast<float>(finest_top_mip), static_cast<float>(mip_tail_start));
            const int32_t top_mip = static_cast<int32_t>(std::floor(clamped));

            const uint8_t desired = static_cast<uint8_t>(metadata.num_mips - static_cast<uint32_t>(top_mip));
            const float coverage = projected_area_fraction(sphere, view, context.viewport_size);

            desired_level = glm::max(desired_level, desired);
            score = glm::max(score, coverage);
        }
    }

    return EvaluateResult{
        .desired_level = desired_level,
        .score = score,
    };
}

bool TextureStreamingManager::is_streamable(TextureAssetHandle handle) const
{
    if (const std::optional<TextureAssetRecord> record = m_asset_loader.get_texture_record(handle); record.has_value())
    {
        const TextureAssetMetadata& metadata = record->metadata;
        return metadata.streamable_mips > 0 && metadata.num_mips > 1;
    }

    MIZU_UNREACHABLE("Failed to get record for TextureAssetHandle: {}", handle.get_id());
    return false;
}

} // namespace Mizu
