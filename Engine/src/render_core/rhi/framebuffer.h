#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>

#include "base/containers/inplace_vector.h"

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

class Framebuffer
{
  public:
    struct Attachment
    {
        std::shared_ptr<RenderTargetView> rtv{};

        LoadOperation load_operation = LoadOperation::Clear;
        StoreOperation store_operation = StoreOperation::DontCare;

        ImageResourceState initial_state = ImageResourceState::Undefined;
        ImageResourceState final_state = ImageResourceState::ShaderReadOnly;

        glm::vec4 clear_value = glm::vec4(0.0f);
    };

    struct Description
    {
        uint32_t width = 0, height = 0;

        inplace_vector<Attachment, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments{};
        std::optional<Attachment> depth_stencil_attachment{};

        std::string_view name = "";
    };

    virtual ~Framebuffer() = default;

    static std::shared_ptr<Framebuffer> create(const Description& desc);

    virtual std::span<const Attachment> get_color_attachments() const = 0;
    virtual std::optional<const Attachment> get_depth_stencil_attachment() const = 0;

    virtual uint32_t get_width() const = 0;
    virtual uint32_t get_height() const = 0;
};

} // namespace Mizu
