#pragma once

#include <glm/glm.hpp>
#include <optional>

#include "base/containers/inplace_vector.h"

#include "mizu_render_core_module.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu
{

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

struct FramebufferInfo
{
    inplace_vector<ImageFormat, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments{};
    std::optional<ImageFormat> depth_stencil_attachment{};
};

struct FramebufferAttachment
{
    ImageResourceView rtv{};

    LoadOperation load_operation = LoadOperation::Clear;
    StoreOperation store_operation = StoreOperation::DontCare;

    glm::vec4 clear_value = glm::vec4(0.0f);

    bool read_only = false;
};

struct RenderPassInfo
{
    inplace_vector<FramebufferAttachment, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments{};
    std::optional<FramebufferAttachment> depth_stencil_attachment{};
};

MIZU_RENDER_CORE_API FramebufferInfo create_framebuffer_info(const RenderPassInfo& info);
MIZU_RENDER_CORE_API glm::uvec2 get_render_pass_extent(const RenderPassInfo& info);

} // namespace Mizu
