#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>

#include "base/containers/inplace_vector.h"

#include "mizu_render_core_module.h"
#include "render_core/rhi/image_resource.h"

namespace Mizu
{

// Forward declarations
class RenderTargetView;

enum class LoadOperation
{
    Load,
    Clear,
    DontCare,
};

enum class StoreOperation
{
    Store,
    DontCare,
    None,
};

inline constexpr size_t MAX_FRAMEBUFFER_COLOR_ATTACHMENTS = 8;

struct FramebufferAttachment
{
    ResourceView rtv{};

    LoadOperation load_operation = LoadOperation::Clear;
    StoreOperation store_operation = StoreOperation::DontCare;

    ImageResourceState initial_state = ImageResourceState::Undefined;
    ImageResourceState final_state = ImageResourceState::ShaderReadOnly;

    glm::vec4 clear_value = glm::vec4(0.0f);
};

struct FramebufferDescription
{
    uint32_t width = 0, height = 0;

    inplace_vector<FramebufferAttachment, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments{};
    std::optional<FramebufferAttachment> depth_stencil_attachment{};

    std::string_view name = "";
};

class Framebuffer
{
  public:
    virtual ~Framebuffer() = default;

    virtual std::span<const FramebufferAttachment> get_color_attachments() const = 0;
    virtual std::optional<const FramebufferAttachment> get_depth_stencil_attachment() const = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu
