#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>

#include "asset/asset_handle.h"
#include "base/debug/assert.h"
#include "base/utils/hash.h"
#include "render_core/rhi/descriptors.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/rhi/render_pass.h"

#include "render/core/camera.h"
#include "render/scene/draw_class.h"

namespace Mizu
{

struct RasterState
{
    RasterizationState rasterization{};
    DepthStencilState depth_stencil{};
    ColorBlendState color_blend{};
};

struct RasterShaders
{
    ShaderAssetHandle vertex{};
    ShaderAssetHandle fragment{};

    bool operator==(const RasterShaders& other) const { return vertex == other.vertex && fragment == other.fragment; }
};

struct DrawFilter
{
    DrawableFlags required = DrawableFlags::None;
    DrawableFlags excluded = DrawableFlags::None;

    bool operator==(const DrawFilter& other) const { return required == other.required && excluded == other.excluded; }
};

struct DrawListRasterBindings
{
    std::array<std::shared_ptr<DescriptorSet>, MAX_DESCRIPTOR_SET_COUNT> descriptor_sets{};

    DrawListRasterBindings& add(uint32_t set, std::shared_ptr<DescriptorSet> descriptor_set)
    {
        MIZU_ASSERT(
            set < MAX_DESCRIPTOR_SET_COUNT,
            "Set is higher than the max descriptor set count ({} >= {})",
            set,
            MAX_DESCRIPTOR_SET_COUNT);

        if (descriptor_sets[set] != nullptr)
        {
            MIZU_ASSERT(false, "Already added descriptor set at set {}", set);
            return *this;
        }

        descriptor_sets[set] = std::move(descriptor_set);
        return *this;
    }
};

struct DrawListRasterPassInfo
{
    DrawListRasterBindings bindings{};

    // Only used in debug to check that the state the list was created with still matches the render
    // pass it ends up being dispatched into.
    [[maybe_unused]] FramebufferInfo framebuffer_info{};
};

constexpr uint8_t DRAW_LIST_LOD_BIAS_LAST_LOD = std::numeric_limits<uint8_t>::max();

struct DrawListView
{
    glm::mat4 view_matrix{1.0f};
    glm::mat4 proj_matrix{1.0f};
    float znear = 0.001f;
};

struct VisibilityDesc
{
    DrawListView view{};
    Frustum frustum{};
    FrustumMask mask{};
    bool has_frustum = false;

    static VisibilityDesc create(const std::optional<DrawListView>& view_opt, FrustumMask frustum_mask)
    {
        VisibilityDesc desc{};
        desc.has_frustum = view_opt.has_value();

        if (view_opt.has_value())
        {
            const glm::vec3 position = glm::vec3(glm::inverse(view_opt->view_matrix)[3]);

            desc.view = *view_opt;
            desc.frustum = Frustum::from_view_projection(view_opt->proj_matrix * view_opt->view_matrix, position);
            desc.mask = frustum_mask;
        }
        else
        {
            desc.mask = FrustumMask{false, false, false, false, false, false};
        }

        return desc;
    }

    bool operator==(const VisibilityDesc& other) const
    {
        if (has_frustum != other.has_frustum)
            return false;

        if (!(mask == other.mask))
            return false;

        if (!has_frustum)
            return true;

        // clang-format off
        return view.view_matrix == other.view.view_matrix
            && view.proj_matrix == other.view.proj_matrix
            && view.znear == other.view.znear;
        // clang-format on
    }

    size_t hash() const
    {
        size_t h = hash_compute(has_frustum, mask.to_uint8());

        if (!has_frustum)
            return h;

        hash_mat4(h, view.view_matrix);
        hash_mat4(h, view.proj_matrix);
        hash_combine(h, float_bits(view.znear));

        return h;
    }

  private:
    static uint32_t float_bits(float value)
    {
        if (value == 0.0f)
            value = 0.0f;

        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));

        return bits;
    }

    static void hash_mat4(size_t& h, const glm::mat4& m)
    {
        for (glm::length_t column = 0; column < 4; ++column)
        {
            hash_combine(
                h, float_bits(m[column].x), float_bits(m[column].y), float_bits(m[column].z), float_bits(m[column].w));
        }
    }
};

struct DrawListKey
{
    static constexpr uint32_t INVALID_VISIBILITY_IDX = std::numeric_limits<uint32_t>::max();

    uint32_t visibility_idx = INVALID_VISIBILITY_IDX;
    uint32_t view_count = 1;
    uint8_t lod_bias = 0;
    DrawableFlags filter_required = DrawableFlags::None;
    DrawableFlags filter_excluded = DrawableFlags::None;
    size_t raster_state_hash = 0;

    bool operator==(const DrawListKey& other) const
    {
        // clang-format off
        return visibility_idx    == other.visibility_idx
            && view_count        == other.view_count
            && lod_bias          == other.lod_bias
            && filter_required   == other.filter_required
            && filter_excluded   == other.filter_excluded
            && raster_state_hash == other.raster_state_hash;
        // clang-format on
    }

    size_t hash() const
    {
        return hash_compute(visibility_idx, view_count, lod_bias, filter_required, filter_excluded, raster_state_hash);
    }
};

} // namespace Mizu
