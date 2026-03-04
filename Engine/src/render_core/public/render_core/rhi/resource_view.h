#pragma once

#include <memory>
#include <optional>

#include "base/utils/hash.h"

namespace Mizu
{

// Forward declarations
enum class ImageFormat;

struct BufferResourceViewDescription
{
    uint64_t offset = 0;
    uint64_t size = 0;

    bool operator==(const BufferResourceViewDescription& other) const
    {
        return offset == other.offset && size == other.size;
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

} // namespace Mizu

template <>
struct std::hash<Mizu::ResourceView>
{
    size_t operator()(const Mizu::ResourceView& view) const
    {
        return Mizu::hash_compute(view.internal, view.view_type);
    }
};
