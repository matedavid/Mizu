#pragma once

#include <memory>

#include "render_core/rhi/resource_view.h"

namespace Mizu::Dx12
{

// Forward declarations
class Dx12ImageResource;

class Dx12ImageResourceView : public ImageResourceView
{
  public:
    Dx12ImageResourceView(std::shared_ptr<ImageResource> resource, ImageResourceViewRange range = {});

    ImageFormat get_format() const override;
    ImageResourceViewRange get_range() const override;

  private:
    std::shared_ptr<Dx12ImageResource> m_resource;
    ImageResourceViewRange m_range;
};

} // namespace Mizu::Dx12