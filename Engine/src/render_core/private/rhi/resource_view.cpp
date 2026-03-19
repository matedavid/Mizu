#include "render_core/rhi/resource_view.h"

#include "render_core/rhi/acceleration_structure.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

//
// BufferResourceView
//

BufferResourceView BufferResourceView::create(std::shared_ptr<BufferResource> buffer_)
{
    MIZU_ASSERT(buffer_ != nullptr, "Trying to pass nullptr");

    const BufferResourceViewDescription view_desc = {
        .offset = 0,
        .size = buffer_->get_size(),
        .stride = buffer_->get_stride(),
    };

    return BufferResourceView::create(buffer_, view_desc);
}

BufferResourceView BufferResourceView::create(
    std::shared_ptr<BufferResource> buffer_,
    const BufferResourceViewDescription& desc_)
{
    return BufferResourceView(buffer_.get(), desc_);
}

//
// ImageResourceView
//

ImageResourceView ImageResourceView::create(std::shared_ptr<ImageResource> image_)
{
    MIZU_ASSERT(image_ != nullptr, "Trying to pass nullptr");

    const ImageResourceViewDescription view_desc = {
        .mip_base = 0,
        .mip_count = image_->get_num_mips(),
        .layer_base = 0,
        .layer_count = image_->get_num_layers(),
    };

    return ImageResourceView::create(image_, view_desc);
}

ImageResourceView ImageResourceView::create(
    std::shared_ptr<ImageResource> image_,
    const ImageResourceViewDescription& desc_)
{
    return ImageResourceView(image_.get(), desc_);
}

//
// AccelerationStructureView
//

AccelerationStructureView AccelerationStructureView::create(std::shared_ptr<AccelerationStructure> accel_struct_)
{
    MIZU_ASSERT(accel_struct_ != nullptr, "Trying to pass nullptr");

    return AccelerationStructureView(accel_struct_.get());
}

} // namespace Mizu
