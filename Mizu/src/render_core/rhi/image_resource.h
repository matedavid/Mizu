#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string_view>
#include <vector>

namespace Mizu
{

// Forward declarations
class IDeviceMemoryAllocator;

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
enum class ImageUsageBits : ImageUsageBitsType {
    None        = 0,
    Attachment  = 1,
    Sampled     = 2,
    Storage     = 4,
    TransferDst = 8,
};
// clang-format on

inline ImageUsageBits operator|(ImageUsageBits a, ImageUsageBits b)
{
    return static_cast<ImageUsageBits>(static_cast<ImageUsageBitsType>(a) | static_cast<ImageUsageBitsType>(b));
}

inline ImageUsageBitsType operator&(ImageUsageBits a, ImageUsageBits b)
{
    return static_cast<ImageUsageBitsType>(a) & static_cast<ImageUsageBitsType>(b);
}

enum class ImageResourceState
{
    Undefined,
    General,
    TransferDst,
    ShaderReadOnly,
    ColorAttachment,
    DepthStencilAttachment,
};

struct ImageDescription
{
    uint32_t width = 1, height = 1, depth = 1;
    ImageType type = ImageType::Image2D;
    ImageFormat format = ImageFormat::RGBA8_SRGB;
    ImageUsageBits usage = ImageUsageBits::None;

    uint32_t num_mips = 1;
    uint32_t num_layers = 1;

    std::string_view name;
};

class ImageResource
{
  public:
    virtual ~ImageResource() = default;

    [[nodiscard]] static std::shared_ptr<ImageResource> create(const ImageDescription& desc,
                                                               std::weak_ptr<IDeviceMemoryAllocator> allocator);

    [[nodiscard]] static std::shared_ptr<ImageResource> create(const ImageDescription& desc,
                                                               const uint8_t* content,
                                                               std::weak_ptr<IDeviceMemoryAllocator> allocator);

    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
    [[nodiscard]] virtual uint32_t get_depth() const = 0;
    [[nodiscard]] virtual ImageType get_image_type() const = 0;
    [[nodiscard]] virtual ImageFormat get_format() const = 0;
    [[nodiscard]] virtual ImageUsageBits get_usage() const = 0;
    [[nodiscard]] virtual uint32_t get_num_mips() const = 0;
    [[nodiscard]] virtual uint32_t get_num_layers() const = 0;
};

namespace ImageUtils
{

[[nodiscard]] bool is_depth_format(ImageFormat format);
[[nodiscard]] uint32_t get_format_size(ImageFormat format);

[[nodiscard]] uint32_t compute_num_mips(uint32_t width, uint32_t height, uint32_t depth);
[[nodiscard]] glm::uvec2 compute_mip_size(uint32_t original_width, uint32_t original_height, uint32_t mip_level);

}; // namespace ImageUtils

} // namespace Mizu