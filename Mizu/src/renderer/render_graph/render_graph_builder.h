#pragma once

#include <vector>

#include "core/uuid.h"
#include "renderer/abstraction/graphics_pipeline.h"
#include "renderer/abstraction/texture.h"
#include "utility/logging.h"

namespace Mizu {

// Forward declarations
class RenderCommandBuffer;

using RGFunction = std::function<void(std::shared_ptr<RenderCommandBuffer> command_buffer)>;
using RGTextureRef = UUID;
using RGFramebufferRef = UUID;

struct RGGraphicsPipelineDescription {
    std::shared_ptr<Shader> shader;

    RasterizationState rasterization{};
    DepthStencilState depth_stencil{};
    ColorBlendState color_blend{};
};

class RenderGraphBuilder {
  public:
    RenderGraphBuilder() = default;

    RGTextureRef create_texture(uint32_t width, uint32_t height, ImageFormat format);
    RGTextureRef create_framebuffer(uint32_t width, uint32_t height, std::vector<RGTextureRef> attachments);

    RGTextureRef register_texture(std::shared_ptr<Texture2D> texture);

    void add_pass(std::string_view name,
                  const RGGraphicsPipelineDescription& pipeline_desc,
                  RGFramebufferRef framebuffer,
                  RGFunction func);

  private:
    struct RGTextureCreateInfo {
        RGTextureRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        ImageFormat format = ImageFormat::BGRA8_SRGB;
    };
    std::vector<RGTextureCreateInfo> m_texture_creation_list;
    std::unordered_map<RGTextureRef, std::shared_ptr<Texture2D>> m_external_textures;

    struct RGFramebufferCreateInfo {
        RGFramebufferRef id;
        uint32_t width = 1;
        uint32_t height = 1;
        std::vector<RGTextureRef> attachments{};
    };
    std::vector<RGFramebufferCreateInfo> m_framebuffer_creation_list;

    struct RGRenderPassCreateInfo {
        std::string name;
        size_t pipeline_desc_id;
        RGFramebufferRef framebuffer_id;
        RGFunction func;
    };
    std::vector<RGRenderPassCreateInfo> m_render_pass_creation_list;

    std::unordered_map<size_t, RGGraphicsPipelineDescription> m_pipeline_descriptions;

    static size_t get_graphics_pipeline_checksum(const RGGraphicsPipelineDescription& desc);

    friend class RenderGraph;
};

} // namespace Mizu