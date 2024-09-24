#include "render_graph.h"

#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/compute_pipeline.h"
#include "renderer/abstraction/framebuffer.h"
#include "renderer/abstraction/graphics_pipeline.h"
#include "renderer/abstraction/render_pass.h"
#include "renderer/abstraction/resource_group.h"
#include "renderer/render_graph/render_graph_dependencies.h"

#include "utility/assert.h"
#include <variant>

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
            for (const auto& rg_info : builder.m_pass_create_info_list) {
                if (rg_info.dependencies.contains_rg_texture2D(info.id)) {
                    usage = usage | ImageUsageBits::Sampled;
                    break;
                }
            }

            // Check if it's used in any compute pass
            for (const auto& rg_info : builder.m_pass_create_info_list) {
                if (rg_info.dependencies.contains_rg_texture2D(info.id)) {
                    usage = usage | ImageUsageBits::Storage;
                    break;
                }
            }

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

    const auto create_resources = [&](const std::shared_ptr<IShader>& shader,
                                      const RenderGraphBuilder::RGPassCreateInfo& info) {
        std::vector<RGResourceMemberInfo> resource_members;

        for (const auto& member : info.members) {
            const auto property_info = shader->get_property(member.mem_name);
            if (!property_info.has_value()) {
                MIZU_LOG_ERROR("Property {} not found in shader for render pass: {}", member.mem_name, info.name);
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

        return rg.create_render_pass_resources(resource_members, shader);
    };

    // Create passes
    std::unordered_map<size_t, std::shared_ptr<GraphicsPipeline>> graphic_pipelines;
    {
        for (const auto& info : builder.m_pass_create_info_list) {
            std::shared_ptr<IShader> shader = nullptr;

            if (info.is_render_pass()) {
                const auto value = std::get<RenderGraphBuilder::RGRenderPassCreateInfo>(info.value);
                shader = value.shader;

                MIZU_ASSERT(framebuffers.contains(value.framebuffer_id),
                            "Framebuffer with id: {} does not exist in RenderGraph for RenderPass: {}",
                            static_cast<UUID::Type>(value.framebuffer_id),
                            info.name);
                auto framebuffer = framebuffers[value.framebuffer_id];

                std::shared_ptr<GraphicsPipeline> graphics_pipeline;
                if (graphic_pipelines.contains(value.pipeline_desc_id)) {
                    graphics_pipeline = graphic_pipelines[value.pipeline_desc_id];
                } else {
                    const RGGraphicsPipelineDescription rg_description =
                        builder.m_pipeline_descriptions.find(value.pipeline_desc_id)->second;

                    GraphicsPipeline::Description description;
                    description.shader = value.shader;
                    description.target_framebuffer = framebuffer;
                    description.rasterization = rg_description.rasterization;
                    description.depth_stencil = rg_description.depth_stencil;
                    description.color_blend = rg_description.color_blend;

                    graphics_pipeline = GraphicsPipeline::create(description);

                    graphic_pipelines.insert({value.pipeline_desc_id, graphics_pipeline});
                }

                // Create resources
                const auto resource_ids = create_resources(shader, info);

                // Create Render Pass
                RenderPass::Description render_pass_desc;
                render_pass_desc.debug_name = info.name;
                render_pass_desc.target_framebuffer = framebuffer;

                const auto render_pass = RenderPass::create(render_pass_desc);

                RGRenderPass render_pass_info;
                render_pass_info.render_pass = render_pass;
                render_pass_info.graphics_pipeline = graphics_pipeline;
                render_pass_info.resource_ids = resource_ids;
                render_pass_info.func = std::move(value.func);

                rg.m_passes.push_back(render_pass_info);

            } else if (info.is_compute_pass()) {
                const auto value = std::get<RenderGraphBuilder::RGComputePassCreateInfo>(info.value);
                shader = value.shader;

                // Create Compute Pipeline
                ComputePipeline::Description description;
                description.shader = value.shader;

                const auto compute_pipeline = ComputePipeline::create(description);

                // Create resources
                const auto resource_ids = create_resources(shader, info);

                // Create Compute Pass
                RGComputePass compute_pass_info;
                compute_pass_info.compute_pipeline = compute_pipeline;
                compute_pass_info.resource_ids = resource_ids;
                compute_pass_info.func = std::move(value.func);

                rg.m_passes.push_back(compute_pass_info);
            }
        }
    }

    return rg;
}

void RenderGraph::execute(const CommandBufferSubmitInfo& submit_info) const {
    m_command_buffer->begin();

    for (const auto& pass : m_passes) {
        if (std::holds_alternative<RGRenderPass>(pass)) {
            execute(std::get<RGRenderPass>(pass));
        } else if (std::holds_alternative<RGComputePass>(pass)) {
            execute(std::get<RGComputePass>(pass));
        }
    }

    m_command_buffer->end();

    m_command_buffer->submit(submit_info);
}

void RenderGraph::execute(const RGRenderPass& pass) const {
    m_command_buffer->begin_render_pass(pass.render_pass);
    m_command_buffer->bind_pipeline(pass.graphics_pipeline);

    for (const auto& id : pass.resource_ids) {
        m_command_buffer->bind_resource_group(m_resource_groups[id], m_resource_groups[id]->currently_baked_set());
    }

    pass.func(m_command_buffer);

    m_command_buffer->end_render_pass(pass.render_pass);
}

void RenderGraph::execute(const RGComputePass& pass) const {
    m_command_buffer->bind_pipeline(pass.compute_pipeline);

    for (const auto& id : pass.resource_ids) {
        m_command_buffer->bind_resource_group(m_resource_groups[id], m_resource_groups[id]->currently_baked_set());
    }

    pass.func(m_command_buffer);
}

std::vector<size_t> RenderGraph::create_render_pass_resources(const std::vector<RGResourceMemberInfo>& members,
                                                              const std::shared_ptr<IShader>& shader) {
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
