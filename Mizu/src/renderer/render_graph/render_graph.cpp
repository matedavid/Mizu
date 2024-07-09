#include "render_graph.h"

#include "utility/assert.h"

#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/framebuffer.h"
#include "renderer/abstraction/render_pass.h"
#include "renderer/render_graph/render_graph_dependencies.h"

namespace Mizu {

std::optional<RenderGraph> RenderGraph::build(const RenderGraphBuilder& builder) {
    RenderGraph rg;

    rg.m_command_buffer = RenderCommandBuffer::create();

    // Create textures
    std::unordered_map<RGTextureRef, std::shared_ptr<Texture2D>> textures;
    {
        for (const auto& [id, texture] : builder.m_external_textures) {
            textures.insert({id, texture});
        }

        for (const auto& info : builder.m_texture_creation_list) {
            ImageUsageBits usage = ImageUsageBits::None;

            // Check if it's used as attachment in any framebuffer
            for (const auto& framebuffer_info : builder.m_framebuffer_creation_list) {
                const auto& attachments = framebuffer_info.attachments;
                if (std::find(attachments.begin(), attachments.end(), info.id) != attachments.end()) {
                    usage = usage | ImageUsageBits::Attachment;
                    break;
                }
            }

            // Check if it's used as a dependency in any render pass
            for (const auto& rg_info : builder.m_render_pass_creation_list) {
                if (rg_info.dependencies.contains_rg_texture2D(info.id)) {
                    usage = usage | ImageUsageBits::Sampled;
                    break;
                }
            }

            // TODO: Check if it's used in any compute render pass
            // usage |= ImageUsageBits::Storage;

            // Create texture
            if (usage == ImageUsageBits::None) {
                MIZU_LOG_WARNING("Can't create texture in RenderGraph because it has no usage");
                continue;
            }

            ImageDescription desc;
            desc.width = info.width;
            desc.height = info.height;
            desc.format = info.format;
            desc.usage = usage;

            auto texture = Texture2D::create(desc);
            textures.insert({info.id, texture});
        }
    }

    // Create framebuffers
    std::unordered_map<RGFramebufferRef, std::shared_ptr<Framebuffer>> framebuffers;
    {
        for (const auto& info : builder.m_framebuffer_creation_list) {
            std::vector<Framebuffer::Attachment> attachments;

            for (const auto& attachment_id : info.attachments) {
                MIZU_ASSERT(textures.contains(attachment_id),
                            "Texture with id: {} not created in RenderGraph",
                            static_cast<UUID::Type>(attachment_id));

                const auto texture = textures[attachment_id];

                const ImageUsageBits texture_usage = texture->get_usage();
                glm::vec3 clear_value(0.0f);

                if (ImageUtils::is_depth_format(texture->get_format())) {
                    clear_value = glm::vec3(1.0f);
                }

                Framebuffer::Attachment attachment;
                attachment.image = texture;
                attachment.load_operation = LoadOperation::Clear; // TODO: Sure about this?
                attachment.store_operation =
                    (texture_usage & ImageUsageBits::Sampled) ? StoreOperation::Store : StoreOperation::DontCare;
                attachment.clear_value = clear_value;

                attachments.push_back(attachment);
            }

            Framebuffer::Description desc;
            desc.width = info.width;
            desc.height = info.height;
            desc.attachments = attachments;

            auto framebuffer = Framebuffer::create(desc);
            framebuffers.insert({info.id, framebuffer});
        }
    }

    // Creat render passes
    std::unordered_map<size_t, std::shared_ptr<GraphicsPipeline>> graphic_pipelines;
    {
        for (const auto& info : builder.m_render_pass_creation_list) {
            MIZU_ASSERT(framebuffers.contains(info.framebuffer_id),
                        "Framebuffer with id: {} does not exist in RenderGraph for RenderPass: {}",
                        static_cast<UUID::Type>(info.framebuffer_id),
                        info.name);
            auto framebuffer = framebuffers[info.framebuffer_id];

            std::shared_ptr<GraphicsPipeline> graphics_pipeline;
            if (graphic_pipelines.contains(info.pipeline_desc_id)) {
                graphics_pipeline = graphic_pipelines[info.pipeline_desc_id];
            } else {
                const RGGraphicsPipelineDescription rg_description =
                    builder.m_pipeline_descriptions.find(info.pipeline_desc_id)->second;

                GraphicsPipeline::Description description;
                description.shader = info.shader;
                description.target_framebuffer = framebuffer;
                description.rasterization = rg_description.rasterization;
                description.depth_stencil = rg_description.depth_stencil;
                description.color_blend = rg_description.color_blend;

                graphics_pipeline = GraphicsPipeline::create(description);

                graphic_pipelines.insert({info.pipeline_desc_id, graphics_pipeline});
            }

            RenderPass::Description render_pass_desc;
            render_pass_desc.debug_name = info.name;
            render_pass_desc.target_framebuffer = framebuffer;

            auto render_pass = RenderPass::create(render_pass_desc);

            RGRenderPass render_pass_info;
            render_pass_info.render_pass = render_pass;
            render_pass_info.graphics_pipeline = graphics_pipeline;
            render_pass_info.func = info.func;

            rg.m_render_passes.push_back(render_pass_info);
        }
    }

    return rg;
}

void RenderGraph::execute(const CommandBufferSubmitInfo& submit_info) const {
    m_command_buffer->begin();

    for (const auto& pass : m_render_passes) {
        m_command_buffer->begin_render_pass(pass.render_pass);
        m_command_buffer->bind_pipeline(pass.graphics_pipeline);

        pass.func(m_command_buffer);

        m_command_buffer->end_render_pass(pass.render_pass);
    }

    m_command_buffer->end();

    m_command_buffer->submit(submit_info);
}

} // namespace Mizu
