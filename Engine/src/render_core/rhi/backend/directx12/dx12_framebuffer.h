#pragma once

#include "render_core/rhi/framebuffer.h"

namespace Mizu::Dx12
{

class Dx12Framebuffer : public Framebuffer
{
  public:
    Dx12Framebuffer(Description desc);

    std::span<const Attachment> get_attachments() const override { return std::span(m_description.attachments); }

    virtual uint32_t get_width() const { return m_description.width; }
    virtual uint32_t get_height() const { return m_description.height; }

  private:
    Description m_description;
};

} // namespace Mizu::Dx12