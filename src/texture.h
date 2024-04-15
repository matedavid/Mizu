#pragma once

#include <memory>

namespace Mizu {

enum class ImageFormat {
    RGBA8_SRGB,
    RGBA16_SFLOAT,
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

struct SamplingOptions {
    ImageFilter minification_filter = ImageFilter::Linear;
    ImageFilter magnification_filter = ImageFilter::Linear;

    ImageAddressMode address_mode_u = ImageAddressMode::Repeat;
    ImageAddressMode address_mode_v = ImageAddressMode::Repeat;
    ImageAddressMode address_mode_w = ImageAddressMode::Repeat;
};

struct ImageDescription {
    uint32_t width = 1, height = 1;
    ImageFormat format = ImageFormat::RGBA8_SRGB;

    SamplingOptions sampling_options{};

    // Usage options
    bool generate_mips = false;
    bool storage = false;
};

class Texture2D {
  public:
    virtual ~Texture2D() = default;

    [[nodiscard]] static std::shared_ptr<Texture2D> create(const ImageDescription& desc);

    [[nodiscard]] virtual ImageFormat get_format() const = 0;
    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
};

class ImageUtils {
  public:
    [[nodiscard]] static bool is_depth_format(ImageFormat format);
    [[nodiscard]] static uint32_t compute_num_mips(uint32_t width, uint32_t height);
};

} // namespace Mizu
