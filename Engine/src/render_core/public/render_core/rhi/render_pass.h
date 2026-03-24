#pragma once

#include <glm/glm.hpp>
#include <optional>

#include "base/containers/inplace_vector.h"

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
};

struct RenderPassInfo
{
    glm::uvec2 extent = {0, 0};
    glm::ivec2 offset = {0, 0};

    float min_depth = 0.0f;
    float max_depth = 1.0f;

    inplace_vector<FramebufferAttachment, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments{};
    std::optional<FramebufferAttachment> depth_stencil_attachment{};
};

} // namespace Mizu
