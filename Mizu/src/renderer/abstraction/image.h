#pragma once

#include <cstdint>

namespace Mizu {

enum class ImageFormat {
    RGBA8_SRGB,
    RGBA8_UNORM,
    RGBA16_SFLOAT,

    BGRA8_SRGB,

    D32_SFLOAT,
};

enum class ImageFilter {
    Nearest,
    Linear,
};

enum class ImageAddressMode {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

using ImageUsageBitsType = uint8_t;

// clang-format off
enum class ImageUsageBits : ImageUsageBitsType {
    None       = 0,
    Attachment = 1,
    Sampled    = 2,
    Storage    = 4,
};
// clang-format on

inline ImageUsageBits operator|(ImageUsageBits a, ImageUsageBits b) {
    return static_cast<ImageUsageBits>(static_cast<ImageUsageBitsType>(a) | static_cast<ImageUsageBitsType>(b));
}

// inline ImageUsageBits operator|=(ImageUsageBits a, ImageUsageBits b) {
//     return a | b;
// }

inline ImageUsageBitsType operator&(ImageUsageBits a, ImageUsageBits b) {
    return static_cast<ImageUsageBitsType>(a) & static_cast<ImageUsageBitsType>(b);
}

// inline ImageUsageBits operator&=(ImageUsageBits a, ImageUsageBits b) {
//     return static_cast<ImageUsageBits>(static_cast<ImageUsageBitsType>(a) & static_cast<ImageUsageBitsType>(b));
// }

struct SamplingOptions {
    ImageFilter minification_filter = ImageFilter::Linear;
    ImageFilter magnification_filter = ImageFilter::Linear;

    ImageAddressMode address_mode_u = ImageAddressMode::Repeat;
    ImageAddressMode address_mode_v = ImageAddressMode::Repeat;
    ImageAddressMode address_mode_w = ImageAddressMode::Repeat;
};

enum class ImageResourceState {
    Undefined,
    General,
    TransferDst,
    ShaderReadOnly,
    ColorAttachment,
    DepthStencilAttachment,
};

struct ImageDescription {
    uint32_t width = 1, height = 1;
    ImageFormat format = ImageFormat::RGBA8_SRGB;
    ImageUsageBits usage = ImageUsageBits::None;

    SamplingOptions sampling_options{};

    bool generate_mips = false;
};

class IImage {
  public:
    virtual ~IImage() = default;

    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
    [[nodiscard]] virtual ImageFormat get_format() const = 0;
    [[nodiscard]] virtual ImageUsageBits get_usage() const = 0;
};

class ImageUtils {
  public:
    [[nodiscard]] static bool is_depth_format(ImageFormat format);
    [[nodiscard]] static uint32_t compute_num_mips(uint32_t width, uint32_t height);
};

} // namespace Mizu