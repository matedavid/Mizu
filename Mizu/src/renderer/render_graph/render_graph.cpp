#include "render_graph.h"

#include <algorithm>
#include <ranges>
#include <variant>

#include "renderer/abstraction/command_buffer.h"
#include "renderer/abstraction/compute_pipeline.h"
#include "renderer/abstraction/framebuffer.h"
#include "renderer/abstraction/graphics_pipeline.h"
#include "renderer/abstraction/render_pass.h"
#include "renderer/abstraction/renderer.h"
#include "renderer/abstraction/resource_group.h"
#include "renderer/abstraction/texture.h"

#include "renderer/render_graph/render_graph_dependencies.h"
#include "renderer/render_graph/render_graph_types.h"

#include "utility/assert.h"

namespace Mizu {

RenderGraph::~RenderGraph() {
    Renderer::wait_idle();
}

void RenderGraph::execute(const CommandBufferSubmitInfo& submit_info) const {
    m_command_buffer->begin();

    for (const auto& pass : m_passes) {
        if (std::holds_alternative<RGRenderPass>(pass)) {
            execute(std::get<RGRenderPass>(pass));
        } else if (std::holds_alternative<RGComputePass>(pass)) {
            execute(std::get<RGComputePass>(pass));
        } else if (std::holds_alternative<RGResourceTransitionPass>(pass)) {
            execute(std::get<RGResourceTransitionPass>(pass));
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

void RenderGraph::execute(const RGResourceTransitionPass& pass) const {
    m_command_buffer->transition_resource(pass.texture, pass.old_state, pass.new_state);
}

//
// RenderGraph Building
//

static std::string to_string(ImageResourceState state) {
    switch (state) {
    case ImageResourceState::Undefined:
        return "Undefined";
    case ImageResourceState::General:
        return "General";
    case ImageResourceState::TransferDst:
        return "TransferDst";
    case ImageResourceState::ShaderReadOnly:
        return "ShaderReadOnly";
    case ImageResourceState::ColorAttachment:
        return "ColorAttachment";
    case ImageResourceState::DepthStencilAttachment:
        return "DepthStencilAttachment";
    }
}

std::optional<RenderGraph> RenderGraph::build(const RenderGraphBuilder& builder) {
    RenderGraph rg;
    rg.m_command_buffer = RenderCommandBuffer::create();

#if MIZU_DEBUG
    MIZU_LOG_INFO("Creating RenderGraph:");
#endif

    // Create textures
    std::unordered_map<RGTextureRef, std::shared_ptr<Texture2D>> textures;
    std::unordered_map<RGTextureRef, std::vector<TextureUsage>> texture_usages;
    {
        for (const auto& [id, texture] : builder.m_external_textures) {
            textures.insert({id, texture});
            texture_usages.insert({id, get_texture_usages(id, builder)});
        }

        for (const auto& info : builder.m_texture_creation_list) {
            const auto usages = get_texture_usages(info.id, builder);
            if (usages.empty()) {
                MIZU_LOG_ERROR("Can't create texture in RenderGraph because it has no usage");
                continue;
            }

            texture_usages.insert({info.id, usages});

            ImageUsageBits usage = ImageUsageBits::None;
            for (const auto& texture_usage : usages) {
                switch (texture_usage.type) {
                case TextureUsage::Type::Attachment:
                    usage = usage | ImageUsageBits::Attachment;
                    break;
                case TextureUsage::Type::SampledDependency:
                    usage = usage | ImageUsageBits::Sampled;
                    break;
                case TextureUsage::Type::StorageDependency:
                    usage = usage | ImageUsageBits::Storage;
                    break;
                }
            }

            MIZU_ASSERT(usage != ImageUsageBits::None, "Texture must have a usage to be created");

#if MIZU_DEBUG
            MIZU_LOG_INFO("    Creating texture ({}) with initial state: {}",
                          static_cast<UUID::Type>(info.id),
                          to_string(ImageResourceState::Undefined));
#endif

            ImageDescription desc;
            desc.width = info.width;
            desc.height = info.height;
            desc.format = info.format;
            desc.usage = usage;

            auto texture = Texture2D::create(desc);
            textures.insert({info.id, texture});

            // If first usage of the texture is as StorageDependency, add Transition to General state
            if (usages[0].type == TextureUsage::Type::StorageDependency) {
                rg.m_passes.push_back(RGResourceTransitionPass{
                    .texture = texture,
                    .old_state = ImageResourceState::Undefined,
                    .new_state = ImageResourceState::General,
                });
            }
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
            // TODO: What if framebuffer is used in multiple render passes??
            //      At the moment ignoring because it's not something that will happen very often (or at least it
            //      shouldn't)
            const auto it = std::ranges::find_if(
                builder.m_pass_create_info_list, [&](RenderGraphBuilder::RGPassCreateInfo pass) -> bool {
                    return pass.is_render_pass()
                           && std::get<RenderGraphBuilder::RGRenderPassCreateInfo>(pass.value).framebuffer_id
                                  == info.id;
                });

            if (it == builder.m_pass_create_info_list.end()) {
                MIZU_LOG_ERROR("Can't create framebuffer in RenderGraph because it's not used in any render pass");
                continue;
            }

            const size_t render_pass_pos = it - builder.m_pass_create_info_list.begin();

            std::vector<Framebuffer::Attachment> attachments;
            for (const RGTextureRef& attachment_ref : info.attachments) {
                MIZU_ASSERT(texture_usages.contains(attachment_ref),
                            "If texture is used as attachment, should have been created and it's usages registered");

                const auto& texture = textures[attachment_ref];
                const auto& usages = texture_usages[attachment_ref];

                const auto& it_usages = std::ranges::find_if(
                    usages, [render_pass_pos](TextureUsage usage) { return usage.render_pass_pos == render_pass_pos; });
                MIZU_ASSERT(it_usages != usages.end(),
                            "If texture is attachment of a framebuffer, should have at least one usage");

                const size_t usage_pos = it_usages - usages.begin();

                ImageResourceState initial_state = ImageResourceState::Undefined;
                LoadOperation load_operation = LoadOperation::Clear;
                if (usage_pos == 0) {
                    // Means that the texture is first used as an attachment of this framebuffer. This means that the
                    // image will have an state of Undefined

                    // initial_state = ImageUtils::is_depth_format(texture->get_format())
                    //                    ? ImageResourceState::DepthStencilAttachment
                    //                    : ImageResourceState::ColorAttachment;
                    initial_state = ImageResourceState::Undefined;
                    load_operation = LoadOperation::Clear;
                } else {
                    const TextureUsage previous_usage = usages[usage_pos - 1];
                    switch (previous_usage.type) {
                    case TextureUsage::Type::Attachment:
                        initial_state = ImageUtils::is_depth_format(texture->get_format())
                                            ? ImageResourceState::DepthStencilAttachment
                                            : ImageResourceState::ColorAttachment;
                        load_operation = LoadOperation::Load;
                        break;
                    case TextureUsage::Type::SampledDependency:
                        // TODO: It's a strange usage, from sample dependency to framebuffer attachment?
                        initial_state = ImageResourceState::ShaderReadOnly;
                        load_operation = LoadOperation::Load;
                        break;
                    case TextureUsage::Type::StorageDependency:
                        initial_state = ImageResourceState::General;
                        load_operation = LoadOperation::Load;
                        break;
                    }
                }

                const bool is_external_texture =
                    std::ranges::find_if(builder.m_external_textures,
                                         [&](std::pair<RGTextureRef, std::shared_ptr<Texture2D>> pair) {
                                             return pair.first == attachment_ref;
                                         })
                    != builder.m_external_textures.end();

                ImageResourceState final_state = ImageResourceState::Undefined;
                StoreOperation store_operation = StoreOperation::DontCare;
                if (usage_pos == usages.size() - 1 && !is_external_texture) {
                    // If it's the last usage of the texture, just keep the initial state and dont care for outpu
                    final_state = initial_state;
                    store_operation = StoreOperation::DontCare;
                } else if (usage_pos == usages.size() - 1 && is_external_texture) {
                    // If it's the last usage of the texture but it's an external texture, store results
                    final_state = ImageResourceState::ShaderReadOnly;
                    store_operation = StoreOperation::Store;
                } else {
                    const TextureUsage next_usage = usages[usage_pos + 1];
                    switch (next_usage.type) {
                    case TextureUsage::Type::Attachment:
                        final_state = ImageUtils::is_depth_format(texture->get_format())
                                          ? ImageResourceState::DepthStencilAttachment
                                          : ImageResourceState::ColorAttachment;
                        store_operation = StoreOperation::Store;
                        break;
                    case TextureUsage::Type::SampledDependency:
                        final_state = (texture->get_usage() & ImageUsageBits::Storage)
                                          ? ImageResourceState::General
                                          : ImageResourceState::ShaderReadOnly;
                        store_operation = StoreOperation::Store;
                        break;
                    case TextureUsage::Type::StorageDependency:
                        final_state = ImageResourceState::General;
                        store_operation = StoreOperation::Store;
                        break;
                    }
                }

#if MIZU_DEBUG
                MIZU_LOG_INFO("    Framebuffer ({}) attachment ({}): {} -> {}",
                              static_cast<UUID::Type>(info.id),
                              static_cast<UUID::Type>(attachment_ref),
                              to_string(initial_state),
                              to_string(final_state));
#endif

                glm::vec3 clear_value(0.0f);
                if (ImageUtils::is_depth_format(texture->get_format())) {
                    clear_value = glm::vec3(1.0f);
                }

                Framebuffer::Attachment attachment;
                attachment.image = texture;
                attachment.load_operation = load_operation;
                attachment.store_operation = store_operation;
                attachment.initial_state = initial_state;
                attachment.final_state = final_state;
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
                const auto& id = std::get<RGTextureRef>(member.value);
                resource_members.push_back(RGResourceMemberInfo{
                    .name = member.mem_name,
                    .set = property_info->binding_info.set,
                    .value = textures[id],
                });
            } break;

            case ShaderDeclarationMemberType::RGUniformBuffer: {
                const auto& id = std::get<RGUniformBufferRef>(member.value);
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
    std::unordered_map<size_t, std::shared_ptr<GraphicsPipeline>> graphics_pipelines;
    {
        for (const auto& info : builder.m_pass_create_info_list) {
            std::shared_ptr<IShader> shader = nullptr;

            if (info.is_render_pass()) {
                const auto& value = std::get<RenderGraphBuilder::RGRenderPassCreateInfo>(info.value);
                shader = value.shader;

                MIZU_ASSERT(framebuffers.contains(value.framebuffer_id),
                            "Framebuffer with id: {} does not exist in RenderGraph for RenderPass: {}",
                            static_cast<UUID::Type>(value.framebuffer_id),
                            info.name);

                const auto& framebuffer = framebuffers[value.framebuffer_id];

                auto it = graphics_pipelines.find(value.pipeline_desc_id);
                if (it == graphics_pipelines.end()) {
                    const RGGraphicsPipelineDescription rg_description =
                        builder.m_pipeline_descriptions.find(value.pipeline_desc_id)->second;

                    GraphicsPipeline::Description description;
                    description.shader = value.shader;
                    description.target_framebuffer = framebuffer;
                    description.rasterization = rg_description.rasterization;
                    description.depth_stencil = rg_description.depth_stencil;
                    description.color_blend = rg_description.color_blend;

                    auto pipeline = GraphicsPipeline::create(description);
                    it = graphics_pipelines.insert({value.pipeline_desc_id, pipeline}).first;
                }

                const std::shared_ptr<GraphicsPipeline> graphics_pipeline = it->second;

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
                render_pass_info.dependencies = info.dependencies;
                render_pass_info.func = value.func;

                rg.m_passes.push_back(render_pass_info);

            } else if (info.is_compute_pass()) {
                const auto& value = std::get<RenderGraphBuilder::RGComputePassCreateInfo>(info.value);
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
                compute_pass_info.dependencies = info.dependencies;
                compute_pass_info.func = value.func;

                rg.m_passes.push_back(compute_pass_info);
            }
        }
    }

    return rg;

    /*
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

    const auto framebuffer_used_in_pass_id = [&](RGFramebufferRef framebuffer) -> size_t {
        for (size_t i = 0; i < builder.m_pass_create_info_list.size(); ++i) {
            const auto& info = builder.m_pass_create_info_list[i];
            if (info.is_render_pass()) {
                auto rp_info = std::get<RenderGraphBuilder::RGRenderPassCreateInfo>(info.value);

                if (rp_info.framebuffer_id == framebuffer)
                    return i;
            }
        }

        MIZU_UNREACHABLE("If framebuffer exists, should be used in a render pass");
    };

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

                ImageResourceState final_state = ImageResourceState::ShaderReadOnly;

                size_t render_pass_id = framebuffer_used_in_pass_id(info.id);
                for (size_t i = render_pass_id; i < builder.m_pass_create_info_list.size(); ++i) {
                    const auto& pass_info = builder.m_pass_create_info_list[i];
                    if (pass_info.dependencies.contains_rg_texture2D(attachment_id)) {
                        if (texture_usage & ImageUsageBits::Storage) {
                            final_state = ImageResourceState::General;
                        } else {
                            final_state = ImageResourceState::ShaderReadOnly;
                        }
                        break;
                    }
                }

                Framebuffer::Attachment attachment;
                attachment.image = texture;
                attachment.load_operation = LoadOperation::Clear; // TODO: Sure about this?
                attachment.store_operation =
                    (texture_usage & ImageUsageBits::Sampled) ? StoreOperation::Store : StoreOperation::DontCare;

                attachment.initial_state = ImageUtils::is_depth_format(texture->get_format())
                                               ? ImageResourceState::DepthStencilAttachment
                                               : ImageResourceState::ColorAttachment;
                attachment.final_state = final_state;

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
                const auto& value = std::get<RenderGraphBuilder::RGRenderPassCreateInfo>(info.value);
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
                render_pass_info.dependencies = info.dependencies;
                render_pass_info.func = value.func;

                rg.m_passes.push_back(render_pass_info);

            } else if (info.is_compute_pass()) {
                const auto& value = std::get<RenderGraphBuilder::RGComputePassCreateInfo>(info.value);
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
                compute_pass_info.dependencies = info.dependencies;
                compute_pass_info.func = value.func;

                rg.m_passes.push_back(compute_pass_info);
            }
        }
    }

    return rg;
    */
}

std::vector<RenderGraph::TextureUsage> RenderGraph::get_texture_usages(RGTextureRef texture,
                                                                       const RenderGraphBuilder& builder) {
    std::vector<TextureUsage> usages;

    for (size_t i = 0; i < builder.m_pass_create_info_list.size(); ++i) {
        const auto& info = builder.m_pass_create_info_list[i];

        if (info.dependencies.contains_rg_texture2D(texture)) {
            std::shared_ptr<IShader> shader = nullptr;
            if (info.is_render_pass()) {
                shader = std::get<RenderGraphBuilder::RGRenderPassCreateInfo>(info.value).shader;
            } else if (info.is_compute_pass()) {
                shader = std::get<RenderGraphBuilder::RGComputePassCreateInfo>(info.value).shader;
            }

            const auto name = info.dependencies.get_dependecy_name(texture);
            MIZU_ASSERT(name.has_value(), "If texture is dependency, should be registered in RenderGraphDependencies");

            const auto property = shader->get_property(*name);
            MIZU_ASSERT(property.has_value() && std::holds_alternative<ShaderTextureProperty>(property->value),
                        "If texture is dependency, should be property of shader");

            TextureUsage::Type usage_type = TextureUsage::Type::SampledDependency;

            const auto& texture_property = std::get<ShaderTextureProperty>(property->value);
            if (texture_property.type == ShaderTextureProperty::Type::Sampled) {
                usage_type = TextureUsage::Type::SampledDependency;
            } else if (texture_property.type == ShaderTextureProperty::Type::Storage) {
                usage_type = TextureUsage::Type::StorageDependency;
            }

            usages.push_back(TextureUsage{
                .type = usage_type,
                .render_pass_pos = i,
            });

        } else if (info.is_render_pass()) {
            const auto& rp_info = std::get<RenderGraphBuilder::RGRenderPassCreateInfo>(info.value);
            const auto& framebuffer_it = std::ranges::find_if(
                builder.m_framebuffer_creation_list, [&](RenderGraphBuilder::RGFramebufferCreateInfo framebuffer_info) {
                    return framebuffer_info.id == rp_info.framebuffer_id;
                });
            MIZU_ASSERT(framebuffer_it != builder.m_framebuffer_creation_list.end(),
                        "If framebuffer is used in render pass, should exist in framebuffer creation list");

            const auto& it = std::ranges::find(framebuffer_it->attachments, texture);
            if (it != framebuffer_it->attachments.end()) {
                usages.push_back(TextureUsage{
                    .type = TextureUsage::Type::Attachment,
                    .render_pass_pos = i,
                });
            }
        }
    }

    return usages;
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

    for (uint32_t set = 0; set < resource_members_per_set.size(); ++set) {
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
                const auto& texture_member = std::get<std::shared_ptr<Texture2D>>(member.value);
                resource_group->add_resource(member.name, texture_member);
            } else if (std::holds_alternative<std::shared_ptr<UniformBuffer>>(member.value)) {
                const auto& ubo_member = std::get<std::shared_ptr<UniformBuffer>>(member.value);
                resource_group->add_resource(member.name, ubo_member);
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
