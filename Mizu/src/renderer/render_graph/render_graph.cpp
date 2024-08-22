#include "render_graph.h"

#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/framebuffer.h"
#include "renderer/abstraction/render_pass.h"
#include "renderer/abstraction/resource_group.h"
#include "renderer/render_graph/render_graph_dependencies.h"

#include "utility/assert.h"

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

            // TODO: Check if it's used in any compute pass
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

    // Create uniform buffers
    std::unordered_map<RGUniformBufferRef, std::shared_ptr<UniformBuffer>> uniform_buffers;
    {
        for (const auto& [id, uniform_buffer] : builder.m_external_uniform_buffers) {
            uniform_buffers.insert({id, uniform_buffer});
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

            // Create resources
            std::vector<size_t> resource_ids;
            {
                std::vector<RGResourceMemberInfo> resource_members;

                for (const auto& member : info.members) {
                    const auto property_info = info.shader->get_property(member.mem_name);
                    if (!property_info.has_value()) {
                        MIZU_LOG_ERROR(
                            "Property {} not found in shader for render pass: {}", member.mem_name, info.name);
                        continue;
                    }

                    switch (member.mem_type) {
                    case ShaderDeclarationMemberType::RGTexture2D: {
                        const auto id = std::get<RGTextureRef>(member.value);
                        resource_members.push_back(RGResourceMemberInfo{
                            .name = member.mem_name,
                            .set = property_info->binding_info.set,
                            .value = textures[id],
                        });
                    } break;

                    case ShaderDeclarationMemberType::RGUniformBuffer: {
                        const auto id = std::get<RGUniformBufferRef>(member.value);
                        resource_members.push_back(RGResourceMemberInfo{
                            .name = member.mem_name,
                            .set = property_info->binding_info.set,
                            .value = uniform_buffers[id],
                        });
                    } break;
                    }
                }

                resource_ids = rg.create_render_pass_resources(resource_members, info.shader);
            }

            RenderPass::Description render_pass_desc;
            render_pass_desc.debug_name = info.name;
            render_pass_desc.target_framebuffer = framebuffer;

            auto render_pass = RenderPass::create(render_pass_desc);

            RGRenderPass render_pass_info;
            render_pass_info.render_pass = render_pass;
            render_pass_info.graphics_pipeline = graphics_pipeline;
            render_pass_info.resource_ids = resource_ids;
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

        for (const auto& id : pass.resource_ids) {
            m_command_buffer->bind_resource_group(m_resource_groups[id], m_resource_groups[id]->currently_baked_set());
        }

        pass.func(m_command_buffer);

        m_command_buffer->end_render_pass(pass.render_pass);
    }

    m_command_buffer->end();

    m_command_buffer->submit(submit_info);
}

std::vector<size_t> RenderGraph::create_render_pass_resources(const std::vector<RGResourceMemberInfo>& members,
                                                              const std::shared_ptr<GraphicsShader>& shader) {
    std::vector<size_t> resource_ids;

    std::vector<std::vector<RGResourceMemberInfo>> resource_members_per_set;
    for (const auto& member : members) {
        if (member.set >= resource_members_per_set.size()) {
            for (size_t i = 0; i < member.set + 1; ++i) {
                resource_members_per_set.emplace_back();
            }
        }

        resource_members_per_set[member.set].push_back(member);
    }

    std::vector<size_t> ids;

    for (size_t set = 0; set < resource_members_per_set.size(); ++set) {
        const auto& resources_set = resource_members_per_set[set];
        if (resources_set.empty())
            continue;

        const size_t checksum = get_resource_members_checksum(resources_set);
        if (m_id_to_resource_group.contains(checksum)) {
            ids.push_back(m_id_to_resource_group[checksum]);
            continue;
        }
        const auto resource_group = ResourceGroup::create();

        for (const auto& member : resources_set) {
            if (std::holds_alternative<std::shared_ptr<Texture2D>>(member.value)) {
                resource_group->add_resource(member.name, std::get<std::shared_ptr<Texture2D>>(member.value));
            } else if (std::holds_alternative<std::shared_ptr<UniformBuffer>>(member.value)) {
                resource_group->add_resource(member.name, std::get<std::shared_ptr<UniformBuffer>>(member.value));
            }
        }

        MIZU_VERIFY(resource_group->bake(shader, set), "Could not bake render pass resource");

        const size_t id = m_resource_groups.size();
        ids.push_back(id);
        m_resource_groups.push_back(resource_group);

        m_id_to_resource_group.insert({checksum, id});
    }

    return ids;
}

size_t RenderGraph::get_resource_members_checksum(const std::vector<RGResourceMemberInfo>& members) {
    size_t checksum = 0;

    for (const auto& member : members) {
        checksum ^= std::hash<std::string>()(member.name);
        checksum ^= std::hash<uint32_t>()(member.set);

        if (std::holds_alternative<std::shared_ptr<Texture2D>>(member.value)) {
            checksum ^= std::hash<Texture2D*>()(std::get<std::shared_ptr<Texture2D>>(member.value).get());
        } else if (std::holds_alternative<std::shared_ptr<UniformBuffer>>(member.value)) {
            checksum ^= std::hash<UniformBuffer*>()(std::get<std::shared_ptr<UniformBuffer>>(member.value).get());
        }
    }

    return checksum;
}

} // namespace Mizu
