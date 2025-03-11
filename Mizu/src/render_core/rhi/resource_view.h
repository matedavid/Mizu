#pragma once

#include <memory>
#include <numeric>

#include "render_core/rhi/image_resource.h"

namespace Mizu
{

class ImageResourceViewRange
{
  public:
    static ImageResourceViewRange from_mips_layers(uint32_t mip_base,
                                                   uint32_t mip_count,
                                                   uint32_t layer_base,
                                                   uint32_t layer_count);
    static ImageResourceViewRange from_mips(uint32_t mip_base, uint32_t mip_count);
    static ImageResourceViewRange from_layers(uint32_t layer_base, uint32_t layer_count);

    uint32_t get_mip_base() const { return m_mip_base; }
    uint32_t get_mip_count() const { return m_mip_count; }
    uint32_t get_layer_base() const { return m_layer_base; }
    uint32_t get_layer_count() const { return m_layer_count; }

    bool operator==(const ImageResourceViewRange& other) const
    {
        return m_mip_base == other.m_mip_base && m_mip_count == other.m_mip_count && m_layer_base == other.m_layer_base
               && m_layer_count == other.m_layer_count;
    }

  private:
    uint32_t m_mip_base = 0, m_mip_count = 1;
    uint32_t m_layer_base = 0, m_layer_count = 1;
};

class ImageResourceView
{
  public:
    virtual ~ImageResourceView() = default;

    [[nodiscard]] static std::shared_ptr<ImageResourceView> create(std::shared_ptr<ImageResource> resource,
                                                                   ImageResourceViewRange range = {});

    [[nodiscard]] virtual ImageFormat get_format() const = 0;
    [[nodiscard]] virtual ImageResourceViewRange get_range() const = 0;
};

} // namespace Mizu
