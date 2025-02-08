#include "render_graph_builder.h"

#include <algorithm>
#include <ranges>

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/resource_group.h"

#include "render_core/render_graph/render_graph_resource_aliasing.h"

namespace Mizu
{

RGCubemapRef RenderGraphBuilder::create_cubemap(glm::vec2 dimensions, ImageFormat format, SamplingOptions sampling)
{
    Cubemap::Description desc{};
    desc.dimensions = dimensions;
    desc.format = format;

    const ImageDescription image_desc = Cubemap::get_image_description(desc);

    RGImageDescription rg_desc{};
    rg_desc.desc = image_desc;
    rg_desc.sampling = sampling;

    auto id = RGCubemapRef();
    m_transient_image_descriptions.insert({id, rg_desc});

    return id;
}

RGCubemapRef RenderGraphBuilder::register_external_cubemap(const Cubemap& cubemap)
{
    auto id = RGTextureRef();
    m_external_images.insert({id, cubemap.get_resource()});

    return id;
}

RGUniformBufferRef RenderGraphBuilder::register_external_buffer(const UniformBuffer& ubo)
{
    auto id = RGUniformBufferRef();
    m_external_buffers.insert({id, ubo.get_resource()});

    return id;
}

RGStorageBufferRef RenderGraphBuilder::register_external_buffer(const StorageBuffer& ssbo)
{
    auto id = RGStorageBufferRef();
    m_external_buffers.insert({id, ssbo.get_resource()});

    return id;
}

RGFramebufferRef RenderGraphBuilder::create_framebuffer(glm::uvec2 dimensions,
                                                        const std::vector<RGTextureRef>& attachments)
{
    RGFramebufferDescription info{};
    info.width = dimensions.x;
    info.height = dimensions.y;
    info.attachments = attachments;

    auto id = RGFramebufferRef();
    m_framebuffer_descriptions.insert({id, info});

    return id;
}

void RenderGraphBuilder::start_debug_label(std::string_view name)
{
    RGPassInfo info{};
    info.func = [name](RenderCommandBuffer& command) {
        command.begin_debug_label(name);
    };
    info.value = RGDebugLabelPassInfo{};

    m_passes.emplace_back(info);
}

void RenderGraphBuilder::end_debug_label()
{
    RGPassInfo info{};
    info.func = [](RenderCommandBuffer& command) {
        command.end_debug_label();
    };
    info.value = RGDebugLabelPassInfo{};

    m_passes.emplace_back(info);
}

RenderGraphDependencies RenderGraphBuilder::create_inputs(const std::vector<ShaderParameterMemberInfo>& members)
{
    RenderGraphDependencies dependencies;

    for (const ShaderParameterMemberInfo& member : members)
    {
        // TODO: Should check values are not invalid
        switch (member.mem_type)
        {
        case ShaderParameterMemberType::RGTexture2D: {
            const RGImageRef& image_ref = std::get<RGTextureRef>(member.value);
            dependencies.add(member.mem_name, image_ref);
        }
        break;
        case ShaderParameterMemberType::RGCubemap: {
            const RGImageRef& image_ref = std::get<RGCubemapRef>(member.value);
            dependencies.add(member.mem_name, image_ref);
        }
        break;
        case ShaderParameterMemberType::RGUniformBuffer: {
            dependencies.add(member.mem_name, std::get<RGUniformBufferRef>(member.value));
        }
        break;
        case ShaderParameterMemberType::RGStorageBuffer: {
            dependencies.add(member.mem_name, std::get<RGStorageBufferRef>(member.value));
        }
        break;
        }
    }

    return dependencies;
}

void RenderGraphBuilder::validate_shader_declaration_members(const IShader& shader,
                                                             const std::vector<ShaderParameterMemberInfo>& members)
{
    const auto properties = shader.get_properties();

    const auto has_member = [&](std::string name, ShaderParameterMemberType type) -> bool {
        return std::find_if(members.begin(),
                            members.end(),
                            [&](const auto& member) { return member.mem_name == name && member.mem_type == type; })
               != members.end();
    };

    bool one_property_not_found = false;

    for (const auto& property : properties)
    {
        bool found = false;

        if (std::holds_alternative<ShaderTextureProperty>(property.value))
        {
            found = has_member(property.name, ShaderParameterMemberType::RGTexture2D)
                    || has_member(property.name, ShaderParameterMemberType::RGCubemap);
        }
        else if (std::holds_alternative<ShaderBufferProperty>(property.value))
        {
            found = has_member(property.name, ShaderParameterMemberType::RGUniformBuffer)
                    || has_member(property.name, ShaderParameterMemberType::RGStorageBuffer);
        }

        if (!found)
        {
            one_property_not_found = true;
            MIZU_LOG_ERROR("Shader property with name '{}' not present in shader declaration", property.name);
        }
    }

    MIZU_ASSERT(!one_property_not_found, "Shader declaration does not match shader");
}

//
// Compilation
//

// Pass Creation

std::optional<RenderGraph> RenderGraphBuilder::compile(std::shared_ptr<RenderCommandBuffer> command,
                                                       RenderGraphDeviceMemoryAllocator& allocator)
{
    RenderGraph rg(command);

    // 1. Compute total size of transient resources
    // 1.1. Get usage of resources, to check dependencies and if some resources can overlap
    // 1.2. Compute total size taking into account possible aliasing

    std::vector<RGResourceLifetime> resources;

    RGImageMap image_resources;
    std::unordered_map<RGImageRef, std::vector<RGImageUsage>> image_usages;

    for (const auto& [id, desc] : m_transient_image_descriptions)
    {
        const auto usages = get_image_usages(id);
        if (usages.empty())
        {
            MIZU_LOG_WARNING("Ignoring image with id {} because no usage was found", static_cast<UUID::Type>(id));
            continue;
        }

        image_usages.insert({id, usages});

        ImageUsageBits usage_bits = ImageUsageBits::None;
        for (const RGImageUsage& usage : usages)
        {
            ImageUsageBits image_usage = ImageUsageBits::None;
            switch (usage.type)
            {
            case RGImageUsage::Type::Attachment:
                image_usage = ImageUsageBits::Attachment;
                break;
            case RGImageUsage::Type::Sampled:
                image_usage = ImageUsageBits::Sampled;
                break;
            case RGImageUsage::Type::Storage:
                image_usage = ImageUsageBits::Storage;
                break;
            }

            usage_bits = usage_bits | image_usage;
        }

        ImageDescription transient_desc = desc.desc;
        transient_desc.usage = usage_bits;

        const auto transient = TransientImageResource::create(transient_desc, desc.sampling);
        image_resources.insert({id, transient->get_resource()});

        RGResourceLifetime lifetime{};
        lifetime.begin = usages[0].render_pass_idx;
        lifetime.end = usages[usages.size() - 1].render_pass_idx;
        lifetime.size = transient->get_size();
        lifetime.value = id;
        lifetime.transient_image = transient;

        resources.push_back(lifetime);
    }

    RGBufferMap buffer_resources;

    for (const auto& [id, desc] : m_transient_buffer_descriptions)
    {
        const auto usages = get_buffer_usages(id);
        if (usages.empty())
        {
            MIZU_LOG_WARNING("Ignoring buffer with id {} because no usage was found", static_cast<UUID::Type>(id));
            continue;
        }

        BufferDescription transient_desc{};
        transient_desc.size = desc.size;
        transient_desc.type = desc.type;

        std::shared_ptr<TransientBufferResource> transient;
        if (desc.data.empty())
        {
            transient = TransientBufferResource::create(transient_desc);
        }
        else
        {
            transient = TransientBufferResource::create(transient_desc, desc.data);
        }

        buffer_resources.insert({id, transient->get_resource()});

        RGResourceLifetime lifetime{};
        lifetime.begin = usages[0].render_pass_idx;
        lifetime.end = usages[usages.size() - 1].render_pass_idx;
        lifetime.size = transient->get_size();
        lifetime.value = id;
        lifetime.transient_buffer = transient;

        resources.push_back(lifetime);
    }

    // 2. Allocate resources using aliasing
    const size_t size = alias_resources(resources);

    for (const RGResourceLifetime& resource : resources)
    {
        if (std::holds_alternative<RGImageRef>(resource.value))
        {
            allocator.allocate_image_resource(*resource.transient_image, resource.offset);
        }
        else if (std::holds_alternative<RGBufferRef>(resource.value))
        {
            allocator.allocate_buffer_resource(*resource.transient_buffer, resource.offset);
        }
    }

    allocator.allocate();
    MIZU_ASSERT(size == allocator.get_size(), "Expected size and allocated size do not match");

    // 3. Add external resources
    for (const auto& [id, image] : m_external_images)
    {
        image_resources.insert({id, image});
        image_usages.insert({id, get_image_usages(id)});
    }

    for (const auto& [id, buffer] : m_external_buffers)
    {
        buffer_resources.insert({id, buffer});
    }

    // 4. Create passes to execute
    std::unordered_map<size_t, RGResourceGroup> checksum_to_resource_group;

    for (size_t pass_idx = 0; pass_idx < m_passes.size(); ++pass_idx)
    {
        const RGPassInfo& pass_info = m_passes[pass_idx];

        // 4.1. Transition pass dependencies
        for (const RGImageRef& image_dependency : pass_info.inputs.get_image_dependencies())
        {
            MIZU_ASSERT(image_usages.contains(image_dependency),
                        "Image with id {} is not registered in image_usages",
                        static_cast<UUID::Type>(image_dependency));
            const std::vector<RGImageUsage>& usages = image_usages[image_dependency];

            MIZU_ASSERT(image_resources.contains(image_dependency),
                        "Image with id {} is not registered in image_usages",
                        static_cast<UUID::Type>(image_dependency));
            const std::shared_ptr<ImageResource>& image = image_resources[image_dependency];

            const auto& it_usages = std::ranges::find_if(
                usages, [pass_idx](RGImageUsage usage) { return usage.render_pass_idx == pass_idx; });
            MIZU_ASSERT(it_usages != usages.end(),
                        "If texture is attachment of a framebuffer, should have at least one usage");

            const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

            const bool image_is_external =
                std::ranges::find_if(
                    m_external_images,
                    [image_dependency](const std::pair<RGTextureRef, std::shared_ptr<ImageResource>>& pair) {
                        return pair.first == image_dependency;
                    })
                != m_external_images.end();

            ImageResourceState initial_state = ImageResourceState::Undefined;
            if (usage_pos == 0)
            {
                if (image_is_external)
                {
                    initial_state = ImageResourceState::ShaderReadOnly;
                }
                else
                {
                    initial_state = ImageResourceState::Undefined;
                }
            }
            else
            {
                const RGImageUsage& previous_usage = usages[usage_pos - 1];

                switch (previous_usage.type)
                {
                case RGImageUsage::Type::Attachment:
                    initial_state = ImageUtils::is_depth_format(image->get_format())
                                        ? ImageResourceState::DepthStencilAttachment
                                        : ImageResourceState::ColorAttachment;
                    break;
                case RGImageUsage::Type::Sampled:
                    initial_state = ImageResourceState::ShaderReadOnly;
                    break;
                case RGImageUsage::Type::Storage:
                    initial_state = ImageResourceState::General;
                    break;
                }
            }

            const RGImageUsage& usage = usages[usage_pos];
            ImageResourceState final_state = ImageResourceState::Undefined;

            switch (usage.type)
            {
            case RGImageUsage::Type::Attachment:
                MIZU_UNREACHABLE("If image is dependency, it can't have a usage of Attachment");
                break;
            case RGImageUsage::Type::Sampled:
                // TODO: Revisit if it's necessary to do this here, I believe this is something that should be handled
                // by the api
                final_state = (image->get_usage() & ImageUsageBits::Storage) ? ImageResourceState::General
                                                                             : ImageResourceState::ShaderReadOnly;
                break;
            case RGImageUsage::Type::Storage:
                final_state = ImageResourceState::General;
                break;
            }

            add_image_transition_pass(rg, *image, initial_state, final_state);
        }

        // 4.2. Create resource group
        std::vector<RGResourceGroup> resource_groups;

        if (pass_info.is_type<RGRenderPassInfo>() || pass_info.is_type<RGComputePassInfo>())
        {
            const auto shader = [&]() -> std::shared_ptr<IShader> {
                if (pass_info.is_type<RGRenderPassInfo>())
                {
                    return pass_info.get_value<RGRenderPassInfo>().shader;
                }
                else if (pass_info.is_type<RGComputePassInfo>())
                {
                    return pass_info.get_value<RGComputePassInfo>().shader;
                }

                MIZU_UNREACHABLE("Not valid pass type");
            }();

            const std::vector<RGResourceMemberInfo>& members =
                create_members(pass_info, image_resources, buffer_resources, shader.get());
            resource_groups = create_resource_groups(members, checksum_to_resource_group, shader);
        }
        else
        {
            const std::vector<RGResourceMemberInfo>& members =
                create_members(pass_info, image_resources, buffer_resources, nullptr);
            resource_groups = create_resource_groups(members, checksum_to_resource_group, nullptr);
        }

        // 4.3 Create pass
        if (pass_info.is_type<RGNullPassInfo>())
        {
            add_null_pass(rg, pass_info.name, resource_groups, pass_info.func);
        }
        else if (pass_info.is_type<RGRenderPassNoPipelineInfo>())
        {
            const RGRenderPassNoPipelineInfo& value = pass_info.get_value<RGRenderPassNoPipelineInfo>();

            MIZU_ASSERT(m_framebuffer_descriptions.contains(value.framebuffer),
                        "Framebuffer with id {} is not registered",
                        static_cast<UUID::Type>(value.framebuffer));
            const RGFramebufferDescription& framebuffer_desc = m_framebuffer_descriptions[value.framebuffer];

            const std::shared_ptr<Framebuffer>& framebuffer =
                create_framebuffer(framebuffer_desc, image_resources, image_usages, pass_idx, rg);

            RenderPass::Description create_render_pass_desc{};
            create_render_pass_desc.target_framebuffer = framebuffer;

            const std::shared_ptr<RenderPass> render_pass = RenderPass::create(create_render_pass_desc);

            add_render_pass_no_pipeline(rg, pass_info.name, render_pass, resource_groups, pass_info.func);
        }
        else if (pass_info.is_type<RGRenderPassInfo>())
        {
            const RGRenderPassInfo& value = pass_info.get_value<RGRenderPassInfo>();

            MIZU_ASSERT(m_framebuffer_descriptions.contains(value.framebuffer),
                        "Framebuffer with id {} is not registered",
                        static_cast<UUID::Type>(value.framebuffer));
            const RGFramebufferDescription& framebuffer_desc = m_framebuffer_descriptions[value.framebuffer];

            const std::shared_ptr<Framebuffer>& framebuffer =
                create_framebuffer(framebuffer_desc, image_resources, image_usages, pass_idx, rg);

            GraphicsPipeline::Description create_pipeline_desc{};
            create_pipeline_desc.shader = value.shader;
            create_pipeline_desc.target_framebuffer = framebuffer;
            create_pipeline_desc.rasterization = value.pipeline_desc.rasterization;
            create_pipeline_desc.depth_stencil = value.pipeline_desc.depth_stencil;
            create_pipeline_desc.color_blend = value.pipeline_desc.color_blend;

            const std::shared_ptr<GraphicsPipeline> pipeline =
                Renderer::get_pipeline_cache()->get_pipeline(create_pipeline_desc);

            RenderPass::Description create_render_pass_desc{};
            create_render_pass_desc.target_framebuffer = framebuffer;

            const std::shared_ptr<RenderPass> render_pass = RenderPass::create(create_render_pass_desc);

            add_render_pass(rg, pass_info.name, render_pass, pipeline, resource_groups, pass_info.func);
        }
        else if (pass_info.is_type<RGComputePassInfo>())
        {
            const auto& value = pass_info.get_value<RGComputePassInfo>();

            ComputePipeline::Description create_pipeline_desc{};
            create_pipeline_desc.shader = value.shader;

            const std::shared_ptr<ComputePipeline> pipeline = ComputePipeline::create(create_pipeline_desc);

            add_compute_pass(rg, pass_info.name, pipeline, resource_groups, pass_info.func);
        }
        else if (pass_info.is_type<RGDebugLabelPassInfo>())
        {
            rg.m_passes.push_back(pass_info.func);
        }
    }

    // 5. Transition external resources
    for (const auto& [id, image] : m_external_images)
    {
        const std::vector<RGImageUsage>& usages = image_usages[id];
        if (usages.empty())
        {
            MIZU_LOG_WARNING("Ignoring external image transition for image with id {} because no usage was found",
                             static_cast<UUID::Type>(id));
            continue;
        }

        ImageResourceState initial_state = ImageResourceState::Undefined;

        switch (usages[usages.size() - 1].type)
        {
        case RGImageUsage::Type::Attachment:
            initial_state = ImageUtils::is_depth_format(image->get_format())
                                ? ImageResourceState::DepthStencilAttachment
                                : ImageResourceState::ColorAttachment;
            break;
        case RGImageUsage::Type::Sampled:
            initial_state = ImageResourceState::ShaderReadOnly;
            break;
        case RGImageUsage::Type::Storage:
            initial_state = ImageResourceState::General;
            break;
        }

        add_image_transition_pass(rg, *image, initial_state, ImageResourceState::ShaderReadOnly);
    }

    return rg;
}

// RenderGraph Passes
// ==================

void RenderGraphBuilder::add_image_transition_pass(RenderGraph& rg,
                                                   ImageResource& image,
                                                   ImageResourceState old_state,
                                                   ImageResourceState new_state) const
{
    if (old_state != new_state)
    {
        rg.m_passes.push_back([&image, old_ = old_state, new_ = new_state](RenderCommandBuffer& _command) {
            _command.transition_resource(image, old_, new_);
        });
    }
}

void RenderGraphBuilder::add_null_pass(RenderGraph& rg,
                                       const std::string& name,
                                       const std::vector<RGResourceGroup>& resource_groups,
                                       const RGFunction& func) const
{
    rg.m_passes.push_back([name, resource_groups, func](RenderCommandBuffer& _command) {
        _command.begin_debug_label(name);
        {
            for (const RGResourceGroup& group : resource_groups)
            {
                _command.bind_resource_group(group.resource_group, group.set);
            }

            func(_command);
        }
        _command.end_debug_label();
    });
}

void RenderGraphBuilder::add_render_pass_no_pipeline(RenderGraph& rg,
                                                     const std::string& name,
                                                     const std::shared_ptr<RenderPass>& render_pass,
                                                     const std::vector<RGResourceGroup>& resource_groups,
                                                     const RGFunction& func) const
{
    rg.m_passes.push_back([name, render_pass, resource_groups, func](RenderCommandBuffer& _command) {
        _command.begin_debug_label(name);
        {
            _command.begin_render_pass(render_pass);

            for (const RGResourceGroup& group : resource_groups)
            {
                _command.bind_resource_group(group.resource_group, group.set);
            }

            func(_command);

            _command.end_render_pass();
        }
        _command.end_debug_label();
    });
}

void RenderGraphBuilder::add_render_pass(RenderGraph& rg,
                                         const std::string& name,
                                         const std::shared_ptr<RenderPass>& render_pass,
                                         const std::shared_ptr<GraphicsPipeline>& pipeline,
                                         const std::vector<RGResourceGroup>& resource_groups,
                                         const RGFunction& func) const
{
    rg.m_passes.push_back([name, render_pass, pipeline, resource_groups, func](RenderCommandBuffer& _command) {
        _command.begin_debug_label(name);
        {
            _command.begin_render_pass(render_pass);
            _command.bind_pipeline(pipeline);

            for (const RGResourceGroup& group : resource_groups)
            {
                _command.bind_resource_group(group.resource_group, group.set);
            }

            func(_command);

            _command.end_render_pass();
        }
        _command.end_debug_label();
    });
}

void RenderGraphBuilder::add_compute_pass(RenderGraph& rg,
                                          const std::string& name,
                                          const std::shared_ptr<ComputePipeline>& pipeline,
                                          const std::vector<RGResourceGroup>& resource_groups,
                                          const RGFunction& func) const
{
    rg.m_passes.push_back([name, pipeline, resource_groups, func](RenderCommandBuffer& _command) {
        _command.begin_debug_label(name);
        {
            _command.bind_pipeline(pipeline);

            for (const RGResourceGroup& group : resource_groups)
            {
                _command.bind_resource_group(group.resource_group, group.set);
            }

            func(_command);
        }
        _command.end_debug_label();
    });
}

// ==================

std::vector<RenderGraphBuilder::RGImageUsage> RenderGraphBuilder::get_image_usages(RGImageRef ref) const
{
    std::vector<RGImageUsage> usages;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        const RGPassInfo& info = m_passes[i];

        if (info.inputs.contains(ref))
        {
            if (info.is_type<RGRenderPassNoPipelineInfo>())
            {
                RGImageUsage usage{};
                usage.type = RGImageUsage::Type::Sampled;
                usage.render_pass_idx = i;
                usage.value = ref;

                usages.push_back(usage);
                continue;
            }
            else if (info.is_type<RGNullPassInfo>())
            {
                RGImageUsage usage{};
                // Using storage because it translates to General resource state
                usage.type = RGImageUsage::Type::Storage;
                usage.render_pass_idx = i;
                usage.value = ref;

                usages.push_back(usage);
                continue;
            }

            std::shared_ptr<IShader> shader = nullptr;
            if (info.is_type<RGRenderPassInfo>())
            {
                shader = info.get_value<RGRenderPassInfo>().shader;
            }
            else if (info.is_type<RGComputePassInfo>())
            {
                shader = info.get_value<RGComputePassInfo>().shader;
            }
            else if (info.is_type<RGRenderPassNoPipelineInfo>())
            {
            }
            MIZU_ASSERT(shader != nullptr, "Info is not of any expected type");

            const auto name = info.inputs.get_dependency_name(ref);
            MIZU_ASSERT(name.has_value(), "If texture is dependency, should be registered in RenderGraphDependencies");

            const auto property = shader->get_property(*name);
            MIZU_ASSERT(property.has_value() && std::holds_alternative<ShaderTextureProperty>(property->value),
                        "If texture is dependency, should be property of shader");

            RGImageUsage::Type usage_type = RGImageUsage::Type::Sampled;

            const auto& texture_property = std::get<ShaderTextureProperty>(property->value);
            if (texture_property.type == ShaderTextureProperty::Type::Sampled)
            {
                usage_type = RGImageUsage::Type::Sampled;
            }
            else if (texture_property.type == ShaderTextureProperty::Type::Storage)
            {
                usage_type = RGImageUsage::Type::Storage;
            }

            RGImageUsage usage{};
            usage.type = usage_type;
            usage.render_pass_idx = i;
            usage.value = ref;

            usages.push_back(usage);
        }
        else if (info.is_type<RGRenderPassInfo>() || info.is_type<RGRenderPassNoPipelineInfo>())
        {
            const RGFramebufferRef& framebuffer = info.is_type<RGRenderPassInfo>()
                                                      ? info.get_value<RGRenderPassInfo>().framebuffer
                                                      : info.get_value<RGRenderPassNoPipelineInfo>().framebuffer;

            const auto it = m_framebuffer_descriptions.find(framebuffer);
            MIZU_ASSERT(it != m_framebuffer_descriptions.end(), "Framebuffer is not registered");

            const RGFramebufferDescription& framebuffer_desc = it->second;
            for (RGTextureRef attachment : framebuffer_desc.attachments)
            {
                if (attachment == ref)
                {
                    RGImageUsage usage{};
                    usage.type = RGImageUsage::Type::Attachment;
                    usage.render_pass_idx = i;
                    usage.value = ref;

                    usages.push_back(usage);
                    break;
                }
            }
        }
    }

    return usages;
}

std::vector<RenderGraphBuilder::RGBufferUsage> RenderGraphBuilder::get_buffer_usages(RGBufferRef ref) const
{
    std::vector<RGBufferUsage> usages;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        const RGPassInfo& info = m_passes[i];

        if (info.inputs.contains(ref))
        {
            RGBufferUsage usage{};
            usage.render_pass_idx = i;

            usages.push_back(usage);
        }
    }

    return usages;
}

std::shared_ptr<Framebuffer> RenderGraphBuilder::create_framebuffer(
    const RGFramebufferDescription& framebuffer_desc,
    const RGImageMap& image_resources,
    const std::unordered_map<RGImageRef, std::vector<RGImageUsage>>& image_usages,
    size_t pass_idx,
    RenderGraph& rg) const
{
    std::vector<Framebuffer::Attachment> attachments;

    for (const RGTextureRef& texture : framebuffer_desc.attachments)
    {
        MIZU_ASSERT(image_resources.contains(texture),
                    "Image with id {} not registered as resource",
                    static_cast<UUID::Type>(texture));
        const std::shared_ptr<ImageResource>& image = image_resources.find(texture)->second;

        MIZU_ASSERT(image->get_width() == framebuffer_desc.width && image->get_height() == framebuffer_desc.height,
                    "Dimensions of one of the attachments do not match with dimensions of framebuffer");

        MIZU_ASSERT(image_usages.contains(texture),
                    "Image with id {} is not registered in image_usages",
                    static_cast<UUID::Type>(texture));
        const std::vector<RGImageUsage>& usages = image_usages.find(texture)->second;

        const auto& it_usages =
            std::ranges::find_if(usages, [pass_idx](RGImageUsage usage) { return usage.render_pass_idx == pass_idx; });
        MIZU_ASSERT(it_usages != usages.end(),
                    "If texture is attachment of a framebuffer, should have at least one usage");

        const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

        const bool image_is_external =
            std::ranges::find_if(m_external_images,
                                 [texture](const std::pair<RGTextureRef, std::shared_ptr<ImageResource>>& pair) {
                                     return pair.first == texture;
                                 })
            != m_external_images.end();

        ImageResourceState initial_state = ImageResourceState::Undefined;
        LoadOperation load_operation = LoadOperation::Clear;

        if (usage_pos == 0)
        {
            // If first usage, it means the texture will have a state of undefined either way
            initial_state = ImageResourceState::Undefined;
            load_operation = LoadOperation::Clear;
        }
        else
        {
            const RGImageUsage& previous_usage = usages[usage_pos - 1];

            switch (previous_usage.type)
            {
            case RGImageUsage::Type::Attachment:
                initial_state = ImageUtils::is_depth_format(image->get_format())
                                    ? ImageResourceState::DepthStencilAttachment
                                    : ImageResourceState::ColorAttachment;

                load_operation = LoadOperation::Load;
                break;
            case RGImageUsage::Type::Sampled:
                initial_state = ImageResourceState::ShaderReadOnly;
                // Loading just in case is a texture that is, for example, a depth buffer that is first read
                // from and then written to again
                load_operation = LoadOperation::Load;
                break;
            case RGImageUsage::Type::Storage:
                initial_state = ImageResourceState::General;
                // The same as Type::Sampled
                load_operation = LoadOperation::Load;
                break;
            }
        }

        ImageResourceState final_state = ImageUtils::is_depth_format(image->get_format())
                                             ? ImageResourceState::DepthStencilAttachment
                                             : ImageResourceState::ColorAttachment;
        StoreOperation store_operation = StoreOperation::Store;

        const bool is_last_usage = usage_pos == usages.size() - 1;
        if (is_last_usage && !image_is_external)
        {
            // If is last usage and is not external, we don't care about store operation
            store_operation = StoreOperation::DontCare;
        }
        else if (is_last_usage && image_is_external)
        {
            // If is last usage and is external, keep the contents
            store_operation = StoreOperation::Store;
        }
        else
        {
            const RGImageUsage next_usage = usages[usage_pos + 1];
            switch (next_usage.type)
            {
            case RGImageUsage::Type::Attachment:
                store_operation = StoreOperation::Store;
                break;
            case RGImageUsage::Type::Sampled:
                store_operation = StoreOperation::Store;
                break;
            case RGImageUsage::Type::Storage:
                store_operation = StoreOperation::Store;
                break;
            }
        }

        glm::vec4 clear_value(0.0f);
        if (ImageUtils::is_depth_format(image->get_format()))
        {
            clear_value = glm::vec4(1.0f);
        }

        Framebuffer::Attachment attachment{};
        attachment.image = std::make_shared<Texture2D>(image);
        attachment.load_operation = load_operation;
        attachment.store_operation = store_operation;
        attachment.initial_state = ImageUtils::is_depth_format(image->get_format())
                                       ? ImageResourceState::DepthStencilAttachment
                                       : ImageResourceState::ColorAttachment;
        attachment.final_state = attachment.initial_state;
        attachment.clear_value = clear_value;

        attachments.push_back(attachment);

        // Prepare image for attachment usage
        add_image_transition_pass(rg, *image, initial_state, final_state);
    }

    Framebuffer::Description create_framebuffer_desc{};
    create_framebuffer_desc.width = framebuffer_desc.width;
    create_framebuffer_desc.height = framebuffer_desc.height;
    create_framebuffer_desc.attachments = attachments;

    return Framebuffer::create(create_framebuffer_desc);
}

std::vector<RenderGraphBuilder::RGResourceMemberInfo> RenderGraphBuilder::create_members(const RGPassInfo& info,
                                                                                         const RGImageMap& images,
                                                                                         const RGBufferMap& buffers,
                                                                                         const IShader* shader) const
{
    std::vector<RGResourceMemberInfo> resource_members;

    uint32_t binding = 0;
    for (const auto& member : info.members)
    {
        uint32_t set = 0;
        if (shader != nullptr)
        {
            const auto property_info = shader->get_property(member.mem_name);
            if (!property_info.has_value())
            {
                MIZU_LOG_ERROR("Property {} not found in shader for render pass: {}", member.mem_name, info.name);
                continue;
            }

            set = property_info->binding_info.set;
            binding = property_info->binding_info.binding;
        }

        switch (member.mem_type)
        {
        case ShaderParameterMemberType::RGTexture2D: {
            const auto& id = std::get<RGTextureRef>(member.value);
            resource_members.push_back(RGResourceMemberInfo{
                .name = member.mem_name,
                .set = set,
                .binding = binding,
                .value = images.find(id)->second,
            });
        }
        break;
        case ShaderParameterMemberType::RGCubemap: {
            const auto& id = std::get<RGCubemapRef>(member.value);
            resource_members.push_back(RGResourceMemberInfo{
                .name = member.mem_name,
                .set = set,
                .binding = binding,
                .value = images.find(id)->second,
            });
        }
        break;
        case ShaderParameterMemberType::RGUniformBuffer: {
            const auto& id = std::get<RGUniformBufferRef>(member.value);
            resource_members.push_back(RGResourceMemberInfo{
                .name = member.mem_name,
                .set = set,
                .binding = binding,
                .value = buffers.find(id)->second,
            });
            break;
        }
        case ShaderParameterMemberType::RGStorageBuffer: {
            const auto& id = std::get<RGStorageBufferRef>(member.value);
            resource_members.push_back(RGResourceMemberInfo{
                .name = member.mem_name,
                .set = set,
                .binding = binding,
                .value = buffers.find(id)->second,
            });
            break;
        }
        }

        if (shader == nullptr)
            binding += 1;
    }

    return resource_members;
}

std::vector<RenderGraphBuilder::RGResourceGroup> RenderGraphBuilder::create_resource_groups(
    const std::vector<RGResourceMemberInfo>& members,
    std::unordered_map<size_t, RGResourceGroup>& checksum_to_resource_group,
    const std::shared_ptr<IShader>& shader) const
{
    const auto get_resource_members_checksum = [&](const std::vector<RGResourceMemberInfo>& _members) -> size_t {
        size_t checksum = 0;

        for (const auto& member : _members)
        {
            checksum ^= std::hash<std::string>()(member.name);
            checksum ^= std::hash<uint32_t>()(member.set);

            if (std::holds_alternative<std::shared_ptr<ImageResource>>(member.value))
            {
                checksum ^= std::hash<ImageResource*>()(std::get<std::shared_ptr<ImageResource>>(member.value).get());
            }
            else if (std::holds_alternative<std::shared_ptr<BufferResource>>(member.value))
            {
                checksum ^= std::hash<BufferResource*>()(std::get<std::shared_ptr<BufferResource>>(member.value).get());
            }
        }

        return checksum;
    };

    std::vector<std::vector<RGResourceMemberInfo>> resource_members_per_set;
    for (const auto& member : members)
    {
        if (member.set >= resource_members_per_set.size())
        {
            for (uint32_t i = 0; i < member.set + 1; ++i)
            {
                resource_members_per_set.emplace_back();
            }
        }

        resource_members_per_set[member.set].push_back(member);
    }

    std::vector<RGResourceGroup> resource_groups;

    for (uint32_t set = 0; set < resource_members_per_set.size(); ++set)
    {
        const auto& resources_set = resource_members_per_set[set];
        if (resources_set.empty())
            continue;

        const size_t checksum = get_resource_members_checksum(resources_set);
        const auto it = checksum_to_resource_group.find(checksum);
        if (it != checksum_to_resource_group.end())
        {
            resource_groups.push_back(it->second);
            continue;
        }

        const auto resource_group = ResourceGroup::create();

        for (const auto& member : resources_set)
        {
            if (std::holds_alternative<std::shared_ptr<ImageResource>>(member.value))
            {
                const auto& image_member = std::get<std::shared_ptr<ImageResource>>(member.value);
                resource_group->add_resource(member.name, image_member);
            }
            else if (std::holds_alternative<std::shared_ptr<BufferResource>>(member.value))
            {
                const auto& buffer_member = std::get<std::shared_ptr<BufferResource>>(member.value);
                resource_group->add_resource(member.name, buffer_member);
            }
        }

        if (shader != nullptr)
        {
            MIZU_VERIFY(resource_group->bake(*shader, set), "Could not bake render pass resource");
        }

        RGResourceGroup rg;
        rg.resource_group = resource_group;
        rg.set = set;

        checksum_to_resource_group.insert({checksum, rg});
        resource_groups.push_back(rg);
    }

    return resource_groups;
}

} // namespace Mizu
