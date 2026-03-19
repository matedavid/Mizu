#pragma once

#include <limits>
#include <memory>
#include <optional>

#include "base/debug/assert.h"
#include "base/utils/hash.h"

#include "mizu_render_core_module.h"

namespace Mizu
{

// Forward declarations
class AccelerationStructure;
class BufferResource;
class ImageResource;
enum class ImageFormat;

struct BufferResourceViewDescription
{
    uint64_t offset = 0;
    uint64_t size = 0;

    bool operator==(const BufferResourceViewDescription& other) const
    {
        return offset == other.offset && size == other.size;
    }

    size_t hash() const
    {
        size_t h = 0;

        hash_combine(h, offset);
        hash_combine(h, size);

        return h;
    }

    bool is_valid(uint64_t buffer_size) const
    {
        return offset < buffer_size && size > 0 && offset + size <= buffer_size;
    }
};

struct ImageResourceViewDescription
{
    uint32_t mip_base = 0;
    uint32_t mip_count = 1;
    uint32_t layer_base = 0;
    uint32_t layer_count = 1;
    std::optional<ImageFormat> override_format = std::nullopt;

    bool operator==(const ImageResourceViewDescription& other) const
    {
        return mip_base == other.mip_base && mip_count == other.mip_count && layer_base == other.layer_base
               && layer_count == other.layer_count && override_format == other.override_format;
    }

    size_t hash() const
    {
        size_t h = 0;

        hash_combine(h, mip_base);
        hash_combine(h, mip_count);
        hash_combine(h, layer_base);
        hash_combine(h, layer_count);

        if (override_format.has_value())
            hash_combine(h, *override_format);
        else
            hash_combine(h, std::numeric_limits<uint32_t>::max());

        return h;
    }

    bool is_valid(uint32_t num_mips, uint32_t num_layers) const
    {
        return mip_base < num_mips && mip_count >= 1 && layer_base < num_layers && layer_count >= 1;
    }
};

enum class ResourceViewType
{
    ShaderResourceView,
    UnorderedAccessView,
    ConstantBufferView,
    RenderTargetView,
};

struct ResourceView
{
    ResourceViewType view_type{};
    void* internal = nullptr;
};

struct BufferResourceView
{
    BufferResource* buffer = nullptr;
    BufferResourceViewDescription desc{};

    BufferResourceView() = default;

    MIZU_RENDER_CORE_API static BufferResourceView create(std::shared_ptr<BufferResource> buffer_);
    MIZU_RENDER_CORE_API static BufferResourceView create(
        std::shared_ptr<BufferResource> buffer_,
        const BufferResourceViewDescription& desc_);

  private:
    BufferResourceView(BufferResource* buffer_, const BufferResourceViewDescription& desc_)
        : buffer(buffer_)
        , desc(desc_)
    {
        MIZU_ASSERT(buffer != nullptr, "Trying to pass nullptr");
    }
};

struct ImageResourceView
{
    ImageResource* image = nullptr;
    ImageResourceViewDescription desc{};

    ImageResourceView() = default;

    MIZU_RENDER_CORE_API static ImageResourceView create(std::shared_ptr<ImageResource> image_);
    MIZU_RENDER_CORE_API static ImageResourceView create(
        std::shared_ptr<ImageResource> image_,
        const ImageResourceViewDescription& desc_);

  private:
    ImageResourceView(ImageResource* image_, const ImageResourceViewDescription& desc_) : image(image_), desc(desc_)
    {
        MIZU_ASSERT(image != nullptr, "Trying to pass nullptr");
    }
};

struct AccelerationStructureView
{
    AccelerationStructure* accel_struct = nullptr;

    AccelerationStructureView() = default;

    MIZU_RENDER_CORE_API static AccelerationStructureView create(std::shared_ptr<AccelerationStructure> accel_struct_);

  private:
    AccelerationStructureView(AccelerationStructure* accel_struct_) : accel_struct(accel_struct_)
    {
        MIZU_ASSERT(accel_struct != nullptr, "Trying to pass nullptr");
    }
};

} // namespace Mizu

template <>
struct std::hash<Mizu::ResourceView>
{
    size_t operator()(const Mizu::ResourceView& view) const
    {
        return Mizu::hash_compute(view.internal, view.view_type);
    }
};
