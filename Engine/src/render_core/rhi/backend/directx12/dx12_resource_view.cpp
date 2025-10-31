#include "dx12_resource_view.h"

#include "render_core/rhi/backend/directx12/dx12_image_resource.h"

namespace Mizu::Dx12
{

Dx12ImageResourceView::Dx12ImageResourceView(std::shared_ptr<ImageResource> resource, ImageResourceViewRange range)
    : m_range(range)
{
    m_resource = std::dynamic_pointer_cast<Dx12ImageResource>(resource);
}

ImageFormat Dx12ImageResourceView::get_format() const
{
    return m_resource->get_format();
}

ImageResourceViewRange Dx12ImageResourceView::get_range() const
{
    return m_range;
}

} // namespace Mizu::Dx12