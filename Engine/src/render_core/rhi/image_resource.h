#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "base/utils/enum_utils.h"
#include "render_core/rhi/device_memory.h"

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

    RG16_SFLOAT,
    RG32_SFLOAT,

    RGB32_SFLOAT,

    RGBA8_SRGB,
    RGBA8_UNORM,

    RGBA16_SFLOAT,
    RGBA32_SFLOAT,

    BGRA8_SRGB,

    D32_SFLOAT,
};

using ImageUsageBitsType = uint8_t;

// clang-format off
enum class ImageUsageBits : ImageUsageBitsType 
{
    None        = 0,
    Attachment  = (1 << 0),
    Sampled     = (1 << 1),
    Storage     = (1 << 2),
    TransferDst = (1 << 3),
};
// clang-format on

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(ImageUsageBits, ImageUsageBitsType);

enum class ImageResourceState
{
    Undefined,
    General,
    TransferDst,
    ShaderReadOnly,
    ColorAttachment,
    DepthStencilAttachment,
    Present,
};

struct ImageDescription
{
    uint32_t width = 1, height = 1, depth = 1;
    ImageType type = ImageType::Image2D;
    ImageFormat format = ImageFormat::RGBA8_SRGB;
    ImageUsageBits usage = ImageUsageBits::None;

    uint32_t num_mips = 1;
    uint32_t num_layers = 1;

    bool is_virtual = false;

    std::string name = "";
};

class ImageResource
{
  public:
    virtual ~ImageResource() = default;

    static std::shared_ptr<ImageResource> create(const ImageDescription& desc);

    virtual MemoryRequirements get_memory_requirements() const = 0;

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

bool is_depth_format(ImageFormat format);
uint32_t get_num_components(ImageFormat format);
uint32_t get_format_size(ImageFormat format);

uint32_t compute_num_mips(uint32_t width, uint32_t height, uint32_t depth);
glm::uvec2 compute_mip_size(uint32_t original_width, uint32_t original_height, uint32_t mip_level);

}; // namespace ImageUtils

} // namespace Mizu