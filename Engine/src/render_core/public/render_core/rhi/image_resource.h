#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>

#include "base/utils/enum_utils.h"

#include "mizu_render_core_module.h"
#include "render_core/definitions/device_memory.h"

namespace Mizu
{

enum class ImageType
{
    Image1D,
    Image2D,
    Image3D,
    Cubemap,
};

enum class ImageFormat
{
    R32_SFLOAT,

    R16G16_SFLOAT,
    R32G32_SFLOAT,

    R32G32B32_SFLOAT,

    R8G8B8A8_SRGB,
    R8G8B8A8_UNORM,

    R16G16B16A16_SFLOAT,
    R32G32B32A32_SFLOAT,

    B8G8R8A8_SRGB,
    B8G8R8A8_UNORM,

    D32_SFLOAT,
};

using ImageUsageBitsType = uint8_t;

// clang-format off
enum class ImageUsageBits : ImageUsageBitsType 
{
    None            = 0,
    Attachment      = (1 << 0),
    Sampled         = (1 << 1),
    UnorderedAccess = (1 << 2),
    TransferDst     = (1 << 3),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(ImageUsageBits, ImageUsageBitsType);

enum class ImageResourceState
{
    Undefined,
    UnorderedAccess,
    TransferDst,
    ShaderReadOnly,
    ColorAttachment,
    DepthStencilAttachment,
    Present,
};

#if MIZU_DEBUG
MIZU_RENDER_CORE_API std::string_view image_resource_state_to_string(ImageResourceState state);
#endif

struct ImageDescription
{
    uint32_t width = 1, height = 1, depth = 1;
    ImageType type = ImageType::Image2D;
    ImageFormat format = ImageFormat::R8G8B8A8_SRGB;
    ImageUsageBits usage = ImageUsageBits::None;

    uint32_t num_mips = 1;
    uint32_t num_layers = 1;

    bool is_virtual = false;

    std::string name = "";
};

class MIZU_RENDER_CORE_API ImageResource
{
  public:
    virtual ~ImageResource() = default;

    static std::shared_ptr<ImageResource> create(const ImageDescription& desc);

    virtual MemoryRequirements get_memory_requirements() const = 0;
    virtual ImageMemoryRequirements get_image_memory_requirements() const = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
    virtual uint32_t get_depth() const = 0;
    virtual ImageType get_image_type() const = 0;
    virtual ImageFormat get_format() const = 0;
    virtual ImageUsageBits get_usage() const = 0;
    virtual uint32_t get_num_mips() const = 0;
    virtual uint32_t get_num_layers() const = 0;

    virtual const std::string& get_name() const = 0;
};

namespace ImageUtils
{

MIZU_RENDER_CORE_API bool is_depth_format(ImageFormat format);
MIZU_RENDER_CORE_API uint32_t get_num_components(ImageFormat format);
MIZU_RENDER_CORE_API uint32_t get_format_size(ImageFormat format);

MIZU_RENDER_CORE_API uint32_t compute_num_mips(uint32_t width, uint32_t height, uint32_t depth);
MIZU_RENDER_CORE_API glm::uvec2 compute_mip_size(uint32_t original_width, uint32_t original_height, uint32_t mip_level);

}; // namespace ImageUtils

} // namespace Mizu