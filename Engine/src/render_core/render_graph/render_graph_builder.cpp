#include "render_graph_builder.h"

#include <algorithm>
#include <ranges>

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/sampler_state.h"

#include "render_core/render_graph/render_graph_resource_aliasing.h"

namespace Mizu
{

RGCubemapRef RenderGraphBuilder::create_cubemap(glm::vec2 dimensions, ImageFormat format, std::string name)
{
    Cubemap::Description cubemap_desc{};
    cubemap_desc.dimensions = dimensions;
    cubemap_desc.format = format;
    cubemap_desc.name = name;

    const ImageDescription image_desc = Cubemap::get_image_description(cubemap_desc);

    RGImageDescription desc{};
    desc.image_desc = image_desc;

    auto id = RGCubemapRef();
    m_transient_image_descriptions.insert({id, desc});

    return id;
}

RGCubemapRef RenderGraphBuilder::create_cubemap(const Cubemap::Description& cubemap_desc)
{
    const ImageDescription image_desc = Cubemap::get_image_description(cubemap_desc);

    RGImageDescription desc{};
    desc.image_desc = image_desc;

    auto id = RGCubemapRef();
    m_transient_image_descriptions.insert({id, desc});

    return id;
}

RGCubemapRef RenderGraphBuilder::register_external_cubemap(const Cubemap& cubemap, RGExternalTextureParams params)
{
    RGExternalImageDescription desc{};
    desc.resource = cubemap.get_resource();
    desc.input_state = params.input_state;
    desc.output_state = params.output_state;

    auto id = RGCubemapRef();
    m_external_image_descriptions.insert({id, desc});

    return id;
}

RGImageViewRef RenderGraphBuilder::create_image_view(RGImageRef image, ImageResourceViewRange range)
{
    RGImageViewDescription desc{};
    desc.image_ref = image;
    desc.range = range;

    auto id = RGImageViewRef();
    m_transient_image_view_descriptions.insert({id, desc});

    return id;
}

RGUniformBufferRef RenderGraphBuilder::register_external_buffer(const UniformBuffer& ubo)
{
    auto id = RGUniformBufferRef();
    m_external_buffers.insert({id, ubo.get_resource()});

    return id;
}

RGStorageBufferRef RenderGraphBuilder::create_storage_buffer(uint64_t size, std::string name)
{
    BufferDescription buffer_desc{};
    buffer_desc.size = size;
    buffer_desc.type = BufferType::StorageBuffer;
    buffer_desc.name = name;

    if (buffer_desc.size == 0)
    {
        // Cannot create empty buffer, if size is zero set to 1
        // TODO: Rethink this approach
        buffer_desc.size = 1;
    }

    RGBufferDescription desc{};
    desc.buffer_desc = buffer_desc;

    auto id = RGStorageBufferRef();
    m_transient_buffer_descriptions.insert({id, desc});

    return id;
}

RGStorageBufferRef RenderGraphBuilder::register_external_buffer(const StorageBuffer& ssbo)
{
    auto id = RGStorageBufferRef();
    m_external_buffers.insert({id, ssbo.get_resource()});

    return id;
}

RGFramebufferRef RenderGraphBuilder::create_framebuffer(glm::uvec2 dimensions,
                                                        const std::vector<RGImageViewRef>& attachments)
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
        case ShaderParameterMemberType::RGImageView: {
            const RGImageViewRef& image_view_ref = std::get<RGImageViewRef>(member.value);
            dependencies.add(member.mem_name, image_view_ref);
        }
        break;
        case ShaderParameterMemberType::RGUniformBuffer: {
            const RGBufferRef& buffer_ref = std::get<RGUniformBufferRef>(member.value);
            dependencies.add(member.mem_name, buffer_ref);
        }
        break;
        case ShaderParameterMemberType::RGStorageBuffer: {
            const RGBufferRef& buffer_ref = std::get<RGStorageBufferRef>(member.value);
            dependencies.add(member.mem_name, buffer_ref);
        }
        break;
        case ShaderParameterMemberType::SamplerState:
            // Do nothing
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

        if (std::holds_alternative<ShaderImageProperty>(property.value))
        {
            found = has_member(property.name, ShaderParameterMemberType::RGImageView);
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

std::optional<RenderGraph> RenderGraphBuilder::compile(RenderGraphDeviceMemoryAllocator& allocator)
{
    RenderGraph rg;

    // 1. Compute total size of transient resources
    // 1.1. Get usage of resources, to check dependencies and if some resources can overlap
    // 1.2. Compute total size taking into account possible aliasing

    std::vector<RGResourceLifetime> resources;

    RGImageMap image_resources;
    std::unordered_map<RGImageRef, std::vector<RGImageUsage>> image_usages;

    std::unordered_map<RGImageRef, std::shared_ptr<BufferResource>> image_to_staging_buffer;

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

        ImageDescription transient_desc = desc.image_desc;
        transient_desc.usage = usage_bits;

        if (!desc.data.empty())
        {
            transient_desc.usage = transient_desc.usage | ImageUsageBits::TransferDst;
        }

        const auto transient = TransientImageResource::create(transient_desc);
        image_resources.insert({id, transient->get_resource()});

        if (!desc.data.empty())
        {
            const std::string staging_name = std::format("Staging_{}", desc.image_desc.name);

            BufferDescription staging_desc{};
            staging_desc.size = desc.data.size();
            staging_desc.type = BufferType::Staging;
            staging_desc.name = staging_name;
            const auto staging_buffer =
                BufferResource::create(staging_desc, desc.data.data(), Renderer::get_allocator());

            image_to_staging_buffer.insert({id, staging_buffer});
        }

        RGResourceLifetime lifetime{};
        lifetime.begin = usages[0].render_pass_idx;
        lifetime.end = usages[usages.size() - 1].render_pass_idx;
        lifetime.size = transient->get_size();
        lifetime.alignment = transient->get_alignment();
        lifetime.value = id;
        lifetime.transient_image = transient;

        resources.push_back(lifetime);
    }

    RGBufferMap buffer_resources;
    std::unordered_map<RGBufferRef, std::vector<RGBufferUsage>> buffer_usages;

    std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>> buffer_to_staging_buffer;

    for (const auto& [id, desc] : m_transient_buffer_descriptions)
    {
        const auto usages = get_buffer_usages(id);
        if (usages.empty())
        {
            MIZU_LOG_WARNING("Ignoring buffer with id {} because no usage was found", static_cast<UUID::Type>(id));
            continue;
        }

        buffer_usages.insert({id, usages});

        const auto transient = TransientBufferResource::create(desc.buffer_desc);
        buffer_resources.insert({id, transient->get_resource()});

        if (!desc.data.empty())
        {
            const std::string staging_name = std::format("Staging_{}", desc.buffer_desc.name);

            BufferDescription staging_desc{};
            staging_desc.size = desc.data.size();
            staging_desc.type = BufferType::Staging;
            staging_desc.name = staging_name;
            const auto staging_buffer =
                BufferResource::create(staging_desc, desc.data.data(), Renderer::get_allocator());

            buffer_to_staging_buffer.insert({id, staging_buffer});
        }

        RGResourceLifetime lifetime{};
        lifetime.begin = usages[0].render_pass_idx;
        lifetime.end = usages[usages.size() - 1].render_pass_idx;
        lifetime.size = transient->get_size();
        lifetime.alignment = transient->get_alignment();
        lifetime.value = id;
        lifetime.transient_buffer = transient;

        resources.push_back(lifetime);
    }

    // 2. Allocate resources using aliasing
    [[maybe_unused]] const size_t size = alias_resources(resources);

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
    for (const auto& [id, desc] : m_external_image_descriptions)
    {
        image_resources.insert({id, desc.resource});
        image_usages.insert({id, get_image_usages(id)});
    }

    for (const auto& [id, buffer] : m_external_buffers)
    {
        buffer_resources.insert({id, buffer});
        buffer_usages.insert({id, get_buffer_usages(id)});
    }

    // 4. Create image views from allocated images
    RGImageViewMap image_view_resources;

    for (const auto& [id, desc] : m_transient_image_view_descriptions)
    {
        const auto it = image_resources.find(desc.image_ref);
        MIZU_ASSERT(it != image_resources.end(),
                    "Image view with id {} references not existent image with id {}",
                    static_cast<UUID::Type>(id),
                    static_cast<UUID::Type>(desc.image_ref));

        const auto image_view = ImageResourceView::create(it->second, desc.range);
        image_view_resources.insert({id, image_view});
    }

    // 5. Create passes to execute
    std::unordered_map<size_t, RGResourceGroup> checksum_to_resource_group;

    for (size_t pass_idx = 0; pass_idx < m_passes.size(); ++pass_idx)
    {
        const RGPassInfo& pass_info = m_passes[pass_idx];

        // 5.1. Transfer staging buffers to resources if it's first usage
        for (const RGImageViewRef& image_view_ref : pass_info.inputs.get_image_view_dependencies())
        {
            const RGImageRef& image_ref = m_transient_image_view_descriptions[image_view_ref].image_ref;

            const std::vector<RGImageUsage>& usages = image_usages[image_ref];
            if (usages[0].render_pass_idx != pass_idx)
            {
                continue;
            }

            const auto it = image_to_staging_buffer.find(image_ref);
            if (it == image_to_staging_buffer.end())
            {
                continue;
            }

            add_copy_to_image_pass(rg, it->second, image_resources[image_ref]);
        }

        for (const RGBufferRef& buffer_ref : pass_info.inputs.get_buffer_dependencies())
        {
            const std::vector<RGBufferUsage>& usages = buffer_usages[buffer_ref];
            if (usages[0].render_pass_idx != pass_idx)
            {
                continue;
            }

            const auto it = buffer_to_staging_buffer.find(buffer_ref);
            if (it == buffer_to_staging_buffer.end())
            {
                continue;
            }

            add_copy_to_buffer_pass(rg, it->second, buffer_resources[buffer_ref]);
        }

        // 5.2. Transition pass dependencies
        for (const RGImageViewRef& image_view_dependency : pass_info.inputs.get_image_view_dependencies())
        {
            MIZU_ASSERT(image_view_resources.contains(image_view_dependency),
                        "Image View with id {} is not registered",
                        static_cast<UUID::Type>(image_view_dependency));
            // TODO: This could cause problems, if we end up adding external image views to the RenderGraph
            const RGImageRef& image_ref = m_transient_image_view_descriptions[image_view_dependency].image_ref;

            MIZU_ASSERT(image_usages.contains(image_ref),
                        "Image with id {} is not registered in image_usages",
                        static_cast<UUID::Type>(image_ref));
            const std::vector<RGImageUsage>& usages = image_usages[image_ref];

            MIZU_ASSERT(image_resources.contains(image_ref),
                        "Image with id {} is not registered in image_usages",
                        static_cast<UUID::Type>(image_ref));
            const std::shared_ptr<ImageResource>& image = image_resources[image_ref];

            const auto& it_usages = std::ranges::find_if(
                usages, [pass_idx](RGImageUsage usage) { return usage.render_pass_idx == pass_idx; });
            MIZU_ASSERT(it_usages != usages.end(),
                        "If texture is attachment of a framebuffer, should have at least one usage");

            const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

            const bool image_is_external =
                std::ranges::find_if(m_external_image_descriptions,
                                     [image_ref](const std::pair<RGImageRef, RGExternalImageDescription>& pair) {
                                         return pair.first == image_ref;
                                     })
                != m_external_image_descriptions.end();

            ImageResourceState initial_state = ImageResourceState::Undefined;
            if (usage_pos == 0)
            {
                if (image_is_external)
                {
                    const RGExternalImageDescription& desc = m_external_image_descriptions[image_ref];
                    initial_state = desc.input_state;
                }
                else if (image->get_usage() & ImageUsageBits::TransferDst)
                {
                    initial_state = ImageResourceState::TransferDst;
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
                final_state = ImageResourceState::ShaderReadOnly;
                break;
            case RGImageUsage::Type::Storage:
                final_state = ImageResourceState::General;
                break;
            }

            add_image_transition_pass(rg, *image, initial_state, final_state);
        }

        // 5.3. Create resource group
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
                create_members(pass_info, image_view_resources, buffer_resources, shader.get());
            resource_groups = create_resource_groups(members, checksum_to_resource_group, shader);
        }
        else
        {
            const std::vector<RGResourceMemberInfo>& members =
                create_members(pass_info, image_view_resources, buffer_resources, nullptr);
            resource_groups = create_resource_groups(members, checksum_to_resource_group, nullptr);
        }

        // 5.4 Create pass
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
                create_framebuffer(framebuffer_desc, image_resources, image_view_resources, image_usages, pass_idx, rg);

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
                create_framebuffer(framebuffer_desc, image_resources, image_view_resources, image_usages, pass_idx, rg);

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

    // 6. Transition external resources
    for (const auto& [id, desc] : m_external_image_descriptions)
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
            initial_state = ImageUtils::is_depth_format(desc.resource->get_format())
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

        add_image_transition_pass(rg, *desc.resource, initial_state, desc.output_state);
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

void RenderGraphBuilder::add_image_transition_pass(RenderGraph& rg,
                                                   ImageResource& image,
                                                   ImageResourceState old_state,
                                                   ImageResourceState new_state,
                                                   ImageResourceViewRange range) const
{
    if (old_state != new_state)
    {
        rg.m_passes.push_back([&image, old_ = old_state, new_ = new_state, range](RenderCommandBuffer& _command) {
            _command.transition_resource(image, old_, new_, range);
        });
    }
}

void RenderGraphBuilder::add_copy_to_image_pass(RenderGraph& rg,
                                                std::shared_ptr<BufferResource> staging,
                                                std::shared_ptr<ImageResource> image)
{
    rg.m_passes.push_back([staging, image](RenderCommandBuffer& _command) {
        _command.transition_resource(*image, ImageResourceState::Undefined, ImageResourceState::TransferDst);
        _command.copy_buffer_to_image(*staging, *image);
    });
}

void RenderGraphBuilder::add_copy_to_buffer_pass(RenderGraph& rg,
                                                 std::shared_ptr<BufferResource> staging,
                                                 std::shared_ptr<BufferResource> buffer)
{
    rg.m_passes.push_back(
        [staging, buffer](RenderCommandBuffer& _command) { _command.copy_buffer_to_buffer(*staging, *buffer); });
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

        RGImageViewRef referenced_by_image_view = RGImageViewRef::invalid();
        for (const RGImageViewRef& view_ref : info.inputs.get_image_view_dependencies())
        {
            const auto it = m_transient_image_view_descriptions.find(view_ref);
            MIZU_ASSERT(it != m_transient_image_view_descriptions.end(),
                        "Transient image view with id {} does not exist",
                        static_cast<UUID::Type>(view_ref));

            if (it->second.image_ref == ref)
            {
                referenced_by_image_view = view_ref;
                break;
            }
        }

        if (referenced_by_image_view != RGImageViewRef::invalid())
        {
            if (info.is_type<RGRenderPassNoPipelineInfo>())
            {
                RGImageUsage usage{};
                usage.type = RGImageUsage::Type::Sampled;
                usage.render_pass_idx = i;
                usage.image = ref;
                usage.view = referenced_by_image_view;

                usages.push_back(usage);
                continue;
            }
            else if (info.is_type<RGNullPassInfo>())
            {
                RGImageUsage usage{};
                // Using storage because it translates to General resource state
                usage.type = RGImageUsage::Type::Storage;
                usage.render_pass_idx = i;
                usage.image = ref;
                usage.view = referenced_by_image_view;

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

            const auto name = info.inputs.get_dependency_name(referenced_by_image_view);
            MIZU_ASSERT(name.has_value(), "If texture is dependency, should be registered in RenderGraphDependencies");

            const auto property = shader->get_property(*name);
            MIZU_ASSERT(property.has_value() && std::holds_alternative<ShaderImageProperty>(property->value),
                        "Shader dependency texture ({}) is not a property of the shader",
                        *name);

            RGImageUsage::Type usage_type = RGImageUsage::Type::Sampled;

            const auto& texture_property = std::get<ShaderImageProperty>(property->value);
            if (texture_property.type == ShaderImageProperty::Type::Sampled)
            {
                usage_type = RGImageUsage::Type::Sampled;
            }
            else if (texture_property.type == ShaderImageProperty::Type::Storage)
            {
                usage_type = RGImageUsage::Type::Storage;
            }

            RGImageUsage usage{};
            usage.type = usage_type;
            usage.render_pass_idx = i;
            usage.image = ref;
            usage.view = referenced_by_image_view;

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
            for (const RGImageViewRef& attachment_view : framebuffer_desc.attachments)
            {
                const auto attachment_it = m_transient_image_view_descriptions.find(attachment_view);
                MIZU_ASSERT(attachment_it != m_transient_image_view_descriptions.end(),
                            "Transient image view with id {} does not exist",
                            static_cast<UUID::Type>(attachment_view));

                if (attachment_it->second.image_ref == ref)
                {
                    RGImageUsage usage{};
                    usage.type = RGImageUsage::Type::Attachment;
                    usage.render_pass_idx = i;
                    usage.image = ref;
                    usage.view = attachment_view;

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
    const RGImageViewMap& image_view_resources,
    const std::unordered_map<RGImageRef, std::vector<RGImageUsage>>& image_usages,
    size_t pass_idx,
    RenderGraph& rg) const
{
    std::vector<Framebuffer::Attachment> attachments;

    for (const RGImageViewRef& view : framebuffer_desc.attachments)
    {
        // TODO: This could cause problems, if we end up adding external image views to the RenderGraph
        MIZU_ASSERT(m_transient_image_view_descriptions.contains(view),
                    "Image view with id {} not registered as resource",
                    static_cast<UUID::Type>(view));
        const RGImageViewDescription& desc = m_transient_image_view_descriptions.find(view)->second;

        MIZU_ASSERT(image_resources.contains(desc.image_ref),
                    "Image view with id {} not registered as resource",
                    static_cast<UUID::Type>(desc.image_ref));
        const std::shared_ptr<ImageResource>& image = image_resources.find(desc.image_ref)->second;

        MIZU_ASSERT(image_view_resources.contains(view),
                    "Image view with id {} not registered as resource",
                    static_cast<UUID::Type>(view));
        const std::shared_ptr<ImageResourceView>& image_view = image_view_resources.find(view)->second;

        MIZU_ASSERT(image_usages.contains(desc.image_ref),
                    "Image with id {} is not registered in image_usages",
                    static_cast<UUID::Type>(desc.image_ref));
        const std::vector<RGImageUsage>& usages = image_usages.find(desc.image_ref)->second;

        const auto& it_usages =
            std::ranges::find_if(usages, [pass_idx](RGImageUsage usage) { return usage.render_pass_idx == pass_idx; });
        MIZU_ASSERT(it_usages != usages.end(),
                    "If texture is attachment of a framebuffer, should have at least one usage");

        const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

        const bool image_is_external =
            std::ranges::find_if(m_external_image_descriptions,
                                 [ref = desc.image_ref](const std::pair<RGImageRef, RGExternalImageDescription>& pair) {
                                     return pair.first == ref;
                                 })
            != m_external_image_descriptions.end();

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

            const ImageResourceViewRange range = image_view_resources.find(view)->second->get_range();
            const ImageResourceViewRange previous_range =
                image_view_resources.find(previous_usage.view)->second->get_range();

            if (range != previous_range)
            {
                // If the previous usage ranges (mip or layer) are different, we need to treat the attachment as it has
                // never been used so that it get's cleared.
                initial_state = ImageResourceState::Undefined;
                load_operation = LoadOperation::Clear;
            }
            else
            {
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
            // Store operation if next usage is an Attachment, Sampled or Storage
            store_operation = StoreOperation::Store;
        }

        glm::vec4 clear_value(0.0f);
        if (ImageUtils::is_depth_format(image->get_format()))
        {
            clear_value = glm::vec4(1.0f);
        }

        Framebuffer::Attachment attachment{};
        attachment.image_view = image_view_resources.find(view)->second;
        attachment.load_operation = load_operation;
        attachment.store_operation = store_operation;
        attachment.initial_state = ImageUtils::is_depth_format(image->get_format())
                                       ? ImageResourceState::DepthStencilAttachment
                                       : ImageResourceState::ColorAttachment;
        attachment.final_state = attachment.initial_state;
        attachment.clear_value = clear_value;

        attachments.push_back(attachment);

        // Prepare image for attachment usage
        add_image_transition_pass(rg, *image, initial_state, final_state, image_view->get_range());
    }

    Framebuffer::Description create_framebuffer_desc{};
    create_framebuffer_desc.width = framebuffer_desc.width;
    create_framebuffer_desc.height = framebuffer_desc.height;
    create_framebuffer_desc.attachments = attachments;

    return Framebuffer::create(create_framebuffer_desc);
}

std::vector<RenderGraphBuilder::RGResourceMemberInfo> RenderGraphBuilder::create_members(
    const RGPassInfo& info,
    const RGImageViewMap& image_views,
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
        case ShaderParameterMemberType::RGImageView: {
            const auto& id = std::get<RGImageViewRef>(member.value);
            resource_members.push_back(RGResourceMemberInfo{
                .name = member.mem_name,
                .set = set,
                .binding = binding,
                .value = image_views.find(id)->second,
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
        case ShaderParameterMemberType::SamplerState: {
            const auto& sampler = std::get<std::shared_ptr<SamplerState>>(member.value);
            resource_members.push_back(RGResourceMemberInfo{
                .name = member.mem_name,
                .set = set,
                .binding = binding,
                .value = sampler,
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
    const auto get_resource_members_checksum = [&](const std::vector<RGResourceMemberInfo>& _members,
                                                   const std::shared_ptr<IShader>& _shader) -> size_t {
        size_t checksum = 0;

        for (const auto& member : _members)
        {
            checksum ^= std::hash<std::string>()(member.name);
            checksum ^= std::hash<uint32_t>()(member.set);

            if (std::holds_alternative<std::shared_ptr<ImageResourceView>>(member.value))
            {
                checksum ^=
                    std::hash<ImageResourceView*>()(std::get<std::shared_ptr<ImageResourceView>>(member.value).get());
            }
            else if (std::holds_alternative<std::shared_ptr<BufferResource>>(member.value))
            {
                checksum ^= std::hash<BufferResource*>()(std::get<std::shared_ptr<BufferResource>>(member.value).get());
            }

            if (_shader != nullptr)
            {
                checksum ^= std::hash<IShader*>()(_shader.get());
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

        // TODO:
        // The `get_resource_members_checksum` method does not take into account the stage (vertex, fragment or
        // compute) where the resource group is being used. So if a resource group is first created for a fragment
        // stage, but the same resource group is later used for a compute stage, it will use the cached value but it
        // won't have a valid descriptor set layout because of the difference in stage.
        // At the moment using hack where the shader is used to compute the checksum, but this is not the best approach
        // because if 2 resource groups have the same stage and members, but are used in different shaders, it will
        // create 2 resource groups instead of using the cached value which in this case it could be reused.
        const size_t checksum = get_resource_members_checksum(resources_set, shader);
        const auto it = checksum_to_resource_group.find(checksum);
        if (it != checksum_to_resource_group.end())
        {
            resource_groups.push_back(it->second);
            continue;
        }

        const auto resource_group = ResourceGroup::create();

        for (const auto& member : resources_set)
        {
            if (std::holds_alternative<std::shared_ptr<ImageResourceView>>(member.value))
            {
                const auto& image_view_member = std::get<std::shared_ptr<ImageResourceView>>(member.value);
                resource_group->add_resource(member.name, image_view_member);
            }
            else if (std::holds_alternative<std::shared_ptr<BufferResource>>(member.value))
            {
                const auto& buffer_member = std::get<std::shared_ptr<BufferResource>>(member.value);
                resource_group->add_resource(member.name, buffer_member);
            }
            else if (std::holds_alternative<std::shared_ptr<SamplerState>>(member.value))
            {
                const auto& sampler_member = std::get<std::shared_ptr<SamplerState>>(member.value);
                resource_group->add_resource(member.name, sampler_member);
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
