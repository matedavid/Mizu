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

struct VisibilityDesc
{
    bool has_frustum = false;
    Frustum frustum{};
    FrustumMask mask{};

    static VisibilityDesc create(const std::optional<Frustum>& frustum_opt, FrustumMask frustum_mask)
    {
        VisibilityDesc desc{};
        desc.has_frustum = frustum_opt.has_value();

        if (frustum_opt.has_value())
        {
            desc.frustum = *frustum_opt;
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
        return planes_equal(frustum.top, other.frustum.top) 
            && planes_equal(frustum.bottom, other.frustum.bottom)
            && planes_equal(frustum.left, other.frustum.left)
            && planes_equal(frustum.right, other.frustum.right)
            && planes_equal(frustum.near, other.frustum.near)
            && planes_equal(frustum.far, other.frustum.far)
            && vec3_equal(frustum.center, other.frustum.center);
        // clang-format on
    }

    size_t hash() const
    {
        size_t h = hash_compute(has_frustum, mask.to_uint8());

        if (!has_frustum)
            return h;

        hash_plane(h, frustum.top);
        hash_plane(h, frustum.bottom);
        hash_plane(h, frustum.left);
        hash_plane(h, frustum.right);
        hash_plane(h, frustum.near);
        hash_plane(h, frustum.far);
        hash_vec3(h, frustum.center);

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

    static bool vec3_equal(const glm::vec3& a, const glm::vec3& b)
    {
        // clang-format off
        return float_bits(a.x) == float_bits(b.x)
            && float_bits(a.y) == float_bits(b.y)
            && float_bits(a.z) == float_bits(b.z);
        // clang-format on
    }

    static bool planes_equal(const Plane& a, const Plane& b)
    {
        return vec3_equal(a.normal, b.normal) && float_bits(a.distance) == float_bits(b.distance);
    }

    static void hash_vec3(size_t& h, const glm::vec3& v)
    {
        hash_combine(h, float_bits(v.x), float_bits(v.y), float_bits(v.z));
    }

    static void hash_plane(size_t& h, const Plane& plane)
    {
        hash_vec3(h, plane.normal);
        hash_combine(h, float_bits(plane.distance));
    }
};

struct DrawListKey
{
    static constexpr uint32_t INVALID_VISIBILITY_IDX = std::numeric_limits<uint32_t>::max();

    uint32_t visibility_idx = INVALID_VISIBILITY_IDX;
    uint32_t view_count = 1;
    DrawableFlags filter_required = DrawableFlags::None;
    DrawableFlags filter_excluded = DrawableFlags::None;
    size_t raster_state_hash = 0;

    bool operator==(const DrawListKey& other) const
    {
        // clang-format off
        return visibility_idx    == other.visibility_idx
            && view_count        == other.view_count
            && filter_required   == other.filter_required
            && filter_excluded   == other.filter_excluded
            && raster_state_hash == other.raster_state_hash;
        // clang-format on
    }

    size_t hash() const
    {
        return hash_compute(
            visibility_idx,
            view_count,
            static_cast<uint32_t>(filter_required),
            static_cast<uint32_t>(filter_excluded),
            raster_state_hash);
    }
};

} // namespace Mizu
