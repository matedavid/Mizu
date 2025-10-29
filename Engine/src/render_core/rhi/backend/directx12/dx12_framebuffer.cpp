#include "dx12_framebuffer.h"

#include "base/debug/assert.h"

namespace Mizu::Dx12
{

Dx12Framebuffer::Dx12Framebuffer(Description desc) : m_description(std::move(desc))
{
    MIZU_ASSERT(!m_description.attachments.empty(), "Empty framebuffer not allowed");
    MIZU_ASSERT(
        m_description.width > 0 && m_description.height > 0,
        "Framebuffer width and height must be greater than 0 (width = {}, height = {})",
        m_description.width,
        m_description.height);
}

} // namespace Mizu::Dx12