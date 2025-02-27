#pragma once

#include <memory>
#include <numeric>

#include "render_core/rhi/image_resource.h"

namespace Mizu
{

class ImageResourceView
{
  public:
    struct Range
    {
      public:
        Range() : m_base(0), m_count(1) {}
        Range(uint32_t base, uint32_t count) : m_base(base), m_count(count) {}

        uint32_t get_base() const { return m_base; }
        uint32_t get_count() const { return m_count; }

      private:
        uint32_t m_base = 0, m_count = 1;
    };

    virtual ~ImageResourceView() = default;

    [[nodiscard]] static std::shared_ptr<ImageResourceView> create(std::shared_ptr<ImageResource> resource,
                                                                   Range mip_range = {},
                                                                   Range layer_range = {});

    [[nodiscard]] virtual ImageFormat get_format() const = 0;
};

} // namespace Mizu
