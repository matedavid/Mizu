#pragma once

#include <memory>

namespace Mizu
{

enum class ImageFilter
{
    Nearest,
    Linear,
};

enum class ImageAddressMode
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

enum class BorderColor
{
    FloatTransparentBlack,
    IntTransparentBlack,
    FloatOpaqueBlack,
    IntOpaqueBlack,
    FloatOpaqueWhite,
    IntOpaqueWhite,
};

struct SamplingOptions
{
    ImageFilter minification_filter = ImageFilter::Linear;
    ImageFilter magnification_filter = ImageFilter::Linear;

    ImageAddressMode address_mode_u = ImageAddressMode::Repeat;
    ImageAddressMode address_mode_v = ImageAddressMode::Repeat;
    ImageAddressMode address_mode_w = ImageAddressMode::Repeat;

    BorderColor border_color = BorderColor::FloatTransparentBlack;
};

class SamplerState
{
  public:
    virtual ~SamplerState() = default;

    [[nodiscard]] static std::shared_ptr<SamplerState> create(SamplingOptions options);
};

} // namespace Mizu