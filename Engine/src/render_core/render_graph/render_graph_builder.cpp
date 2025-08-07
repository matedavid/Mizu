#include "render_graph_builder.h"

#include <algorithm>
#include <ranges>

#include "base/debug/profiling.h"

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
    BufferDescription buffer_desc = StorageBuffer::get_buffer_description(size, name);

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

RGStorageBufferRef RenderGraphBuilder::create_storage_buffer(BufferDescription buffer_desc)
{
    if (buffer_desc.size == 0)
    {
        // Cannot create empty buffer, if size is zero set to 1
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

RGAccelerationStructureRef RenderGraphBuilder::register_external_acceleration_structure(
    std::shared_ptr<AccelerationStructure> as)
{
    auto id = RGAccelerationStructureRef();
    m_external_as.insert({id, as});

    return id;
}

RGResourceGroupRef RenderGraphBuilder::create_resource_group(const RGResourceGroupLayout& layout)
{
    const size_t hash = layout.hash();

    auto it = m_resource_group_cache.find(hash);
    if (it != m_resource_group_cache.end())
        return it->second;

    auto id = RGResourceGroupRef();
    m_resource_group_descriptions.insert({id, layout});
    m_resource_group_cache.insert({hash, id});

    return id;
}

void RenderGraphBuilder::begin_gpu_marker(std::string name)
{
    RGBuilderPass pass(
        "",
        _BaseParameters{},
        RGPassHint::Immediate,
        [name](CommandBuffer& command, [[maybe_unused]] const RGPassResources& params) {
            command.begin_gpu_marker(name);
        });

    m_passes.push_back(pass);
}

void RenderGraphBuilder::end_gpu_marker()
{
    RGBuilderPass pass(
        "",
        _BaseParameters{},
        RGPassHint::Immediate,
        [](CommandBuffer& command, [[maybe_unused]] const RGPassResources& params) { command.end_gpu_marker(); });

    m_passes.push_back(pass);
}

bool RenderGraphBuilder::image_view_references_image(RGImageViewRef view_ref, RGImageRef image_ref) const
{
    const auto it = m_transient_image_view_descriptions.find(view_ref);
    MIZU_ASSERT(
        it != m_transient_image_view_descriptions.end(),
        "Transient image view with id {} does not exist",
        static_cast<UUID::Type>(view_ref));

    return it->second.image_ref == image_ref;
}

//
// Compilation
//

RenderGraph RenderGraphBuilder::compile(RenderGraphDeviceMemoryAllocator& allocator)
{
    MIZU_PROFILE_SCOPED;

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
        const std::vector<RGImageUsage> usages = get_image_usages(id);
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

            usage_bits |= image_usage;
        }

        ImageDescription transient_desc = desc.image_desc;
        transient_desc.usage = usage_bits;

        if (!desc.data.empty())
        {
            transient_desc.usage |= ImageUsageBits::TransferDst;
        }

        const auto transient = TransientImageResource::create(transient_desc);
        image_resources.insert({id, transient->get_resource()});

        if (!desc.data.empty())
        {
            const std::string staging_name = std::format("Staging_{}", desc.image_desc.name);

            // TODO: Don't use Renderer::get_allocator()
            const auto staging_buffer =
                StagingBuffer::create(desc.data.size(), desc.data.data(), Renderer::get_allocator(), staging_name);

            image_to_staging_buffer.insert({id, staging_buffer->get_resource()});
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
        const std::vector<RGBufferUsage> usages = get_buffer_usages(id);
        if (usages.empty())
        {
            MIZU_LOG_WARNING("Ignoring buffer with id {} because no usage was found", static_cast<UUID::Type>(id));
            continue;
        }

        buffer_usages.insert({id, usages});

        BufferUsageBits buffer_usage = BufferUsageBits::None;
        if (!desc.data.empty())
        {
            buffer_usage |= BufferUsageBits::TransferDst;
        }

        BufferDescription transient_desc = desc.buffer_desc;
        transient_desc.usage |= buffer_usage;

        const auto transient = TransientBufferResource::create(transient_desc);
        buffer_resources.insert({id, transient->get_resource()});

        if (!desc.data.empty())
        {
            const std::string staging_name = std::format("Staging_{}", desc.buffer_desc.name);

            // TODO: Don't use Renderer::get_allocator()
            const auto staging_buffer =
                StagingBuffer::create(desc.data.size(), desc.data.data(), Renderer::get_allocator(), staging_name);

            buffer_to_staging_buffer.insert({id, staging_buffer->get_resource()});
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

    RGAccelerationStructureMap acceleration_structure_resources;

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
    MIZU_ASSERT(
        size == allocator.get_size(),
        "Expected size and allocated size do not match ({} != {})",
        size,
        allocator.get_size());

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

    for (const auto& [id, as] : m_external_as)
    {
        acceleration_structure_resources.insert({id, as});
    }

    // 4. Create image views from allocated images
    RGImageViewMap image_view_resources;

    for (const auto& [id, desc] : m_transient_image_view_descriptions)
    {
        const auto it = image_resources.find(desc.image_ref);
        MIZU_ASSERT(
            it != image_resources.end(),
            "Image view with id {} references not existent image with id {}",
            static_cast<UUID::Type>(id),
            static_cast<UUID::Type>(desc.image_ref));

        const auto image_view = ImageResourceView::create(it->second, desc.range);
        image_view_resources.insert({id, image_view});
    }

    // 5. Create resource groups
    RGSResourceGroupMap resource_group_resources;

    for (const auto& [id, desc] : m_resource_group_descriptions)
    {
        ResourceGroupBuilder builder;

        for (const RGResourceGroupLayoutResource& resource : desc.get_resources())
        {
            ResourceGroupItem item{};

            if (resource.is_type<RGLayoutResourceImageView>())
            {
                const RGLayoutResourceImageView& value = resource.as_type<RGLayoutResourceImageView>();
                const std::shared_ptr<ImageResourceView>& view = image_view_resources[value.value];

                switch (value.type)
                {
                case ShaderImageProperty::Type::Sampled:
                case ShaderImageProperty::Type::Separate:
                    item = ResourceGroupItem::SampledImage(resource.binding, view, resource.stage);
                    break;
                case ShaderImageProperty::Type::Storage:
                    item = ResourceGroupItem::StorageImage(resource.binding, view, resource.stage);
                    break;
                }
            }
            else if (resource.is_type<RGLayoutResourceBuffer>())
            {
                const RGLayoutResourceBuffer& value = resource.as_type<RGLayoutResourceBuffer>();
                const std::shared_ptr<BufferResource>& buffer = buffer_resources[value.value];

                switch (value.type)
                {
                case ShaderBufferProperty::Type::Uniform:
                    item = ResourceGroupItem::UniformBuffer(resource.binding, buffer, resource.stage);
                    break;
                case ShaderBufferProperty::Type::Storage:
                    item = ResourceGroupItem::StorageBuffer(resource.binding, buffer, resource.stage);
                    break;
                }
            }
            else if (resource.is_type<RGLayoutResourceSamplerState>())
            {
                const RGLayoutResourceSamplerState& value = resource.as_type<RGLayoutResourceSamplerState>();
                item = ResourceGroupItem::Sampler(resource.binding, value.value, resource.stage);
            }
            else if (resource.is_type<RGLayoutResourceAccelerationStructure>())
            {
                const RGLayoutResourceAccelerationStructure& value =
                    resource.as_type<RGLayoutResourceAccelerationStructure>();
                const std::shared_ptr<AccelerationStructure>& as = acceleration_structure_resources[value.value];

                item = ResourceGroupItem::RtxAccelerationStructure(resource.binding, as, resource.stage);
            }
            else
            {
                MIZU_UNREACHABLE("Invalid or not implemented resource type");
            }

            builder.add_resource(item);
        }

        const auto resource_group = ResourceGroup::create(builder);
        resource_group_resources.insert({id, resource_group});
    }

    // 6. Create passes to execute
    for (size_t pass_idx = 0; pass_idx < m_passes.size(); ++pass_idx)
    {
        const RGBuilderPass& pass = m_passes[pass_idx];

        // 6.1 Create pass resources container
        RGPassResources pass_resources;
        pass_resources.set_resource_group_map(resource_group_resources);

        for (const ShaderParameterMemberInfo& info : pass.get_image_view_members())
        {
            const RGImageViewRef view_ref = std::get<RGImageViewRef>(info.value);
            pass_resources.add_image_view(view_ref, image_view_resources.find(view_ref)->second);
        }

        for (const ShaderParameterMemberInfo& info : pass.get_buffer_members())
        {
            if (info.mem_type == ShaderParameterMemberType::RGUniformBuffer)
            {
                const RGUniformBufferRef ref = std::get<RGUniformBufferRef>(info.value);
                pass_resources.add_buffer(ref, buffer_resources.find(ref)->second);
            }
            else if (info.mem_type == ShaderParameterMemberType::RGStorageBuffer)
            {
                const RGStorageBufferRef ref = std::get<RGStorageBufferRef>(info.value);
                pass_resources.add_buffer(ref, buffer_resources.find(ref)->second);
            }
        }

        // 6.2. If RGPassHint is Immediate, just execute the function
        if (pass.get_hint() == RGPassHint::Immediate)
        {
            add_pass(rg, pass.get_name(), pass_resources, pass.get_function());

            continue;
        }

        // 6.3. Transfer staging buffers to resources if it's first usage
        for (const ShaderParameterMemberInfo& info : pass.get_image_view_members())
        {
            const RGImageViewRef& view_ref = std::get<RGImageViewRef>(info.value);

            const RGImageRef& image_ref = m_transient_image_view_descriptions[view_ref].image_ref;

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

        for (const ShaderParameterMemberInfo& info : pass.get_buffer_members())
        {
            RGBufferRef buffer_ref;
            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGUniformBuffer:
                buffer_ref = std::get<RGUniformBufferRef>(info.value);
                break;
            case ShaderParameterMemberType::RGStorageBuffer:
                buffer_ref = std::get<RGStorageBufferRef>(info.value);
                break;
            default:
                MIZU_UNREACHABLE("ShaderParameterMemberType is not a buffer type");
            }

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

        // 6.4. Transition pass dependencies
        for (const ShaderParameterMemberInfo& info : pass.get_image_view_members())
        {
            const RGImageViewRef& image_view_dependency = std::get<RGImageViewRef>(info.value);

            MIZU_ASSERT(
                image_view_resources.contains(image_view_dependency),
                "Image View with id {} is not registered",
                static_cast<UUID::Type>(image_view_dependency));
            // TODO: This could cause problems, if we end up adding external image views to the RenderGraph
            const RGImageRef& image_ref = m_transient_image_view_descriptions[image_view_dependency].image_ref;

            MIZU_ASSERT(
                image_usages.contains(image_ref),
                "Image with id {} is not registered in image_usages",
                static_cast<UUID::Type>(image_ref));
            const std::vector<RGImageUsage>& usages = image_usages[image_ref];

            MIZU_ASSERT(
                image_resources.contains(image_ref),
                "Image with id {} is not registered in image_usages",
                static_cast<UUID::Type>(image_ref));
            const std::shared_ptr<ImageResource>& image = image_resources[image_ref];

            const auto& it_usages = std::ranges::find_if(
                usages, [pass_idx](RGImageUsage usage) { return usage.render_pass_idx == pass_idx; });
            MIZU_ASSERT(
                it_usages != usages.end(), "If texture is attachment of a framebuffer, should have at least one usage");

            const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

            const bool image_is_external =
                std::ranges::find_if(
                    m_external_image_descriptions,
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

        // 6.5 Create framebuffer
        if (pass.has_framebuffer())
        {
            if (pass.get_hint() != RGPassHint::Raster)
            {
                MIZU_LOG_WARNING("A framebuffer only has effect if the pass has the RGPassHint::Raster hint");
            }

            const RGFramebufferAttachments& rg_framebuffer = pass.get_framebuffer();
            MIZU_ASSERT(
                rg_framebuffer.width != 0 && rg_framebuffer.height != 0,
                "Framebuffer for pass '{}' has invalid width or height (width = {}, height = {})",
                pass.get_name(),
                rg_framebuffer.width,
                rg_framebuffer.height);

            Framebuffer::Description framebuffer_desc{};
            framebuffer_desc.width = rg_framebuffer.width;
            framebuffer_desc.height = rg_framebuffer.height;

            for (const RGImageViewRef& attachment_view : rg_framebuffer.color_attachments)
            {
                framebuffer_desc.attachments.push_back(create_framebuffer_attachment(
                    rg, attachment_view, image_resources, image_view_resources, image_usages, pass_idx));
            }

            if (rg_framebuffer.depth_stencil_attachment != RGImageViewRef::invalid())
            {
                framebuffer_desc.attachments.push_back(create_framebuffer_attachment(
                    rg,
                    rg_framebuffer.depth_stencil_attachment,
                    image_resources,
                    image_view_resources,
                    image_usages,
                    pass_idx));
            }

            const auto framebuffer = Framebuffer::create(framebuffer_desc);
            pass_resources.set_framebuffer(framebuffer);
        }

        // 6.6 Create pass
        add_pass(rg, pass.get_name(), pass_resources, pass.get_function());
    }

    // 7. Transition external resources
    for (const auto& [id, desc] : m_external_image_descriptions)
    {
        const std::vector<RGImageUsage>& usages = image_usages[id];
        if (usages.empty())
        {
            MIZU_LOG_WARNING(
                "Ignoring external image transition for image with id {} because no usage was found",
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

void RenderGraphBuilder::add_image_transition_pass(
    RenderGraph& rg,
    ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state) const
{
    if (old_state != new_state)
    {
        rg.m_passes.push_back([&image, old_ = old_state, new_ = new_state](CommandBuffer& _command) {
            _command.transition_resource(image, old_, new_);
        });
    }
}

void RenderGraphBuilder::add_image_transition_pass(
    RenderGraph& rg,
    ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state,
    ImageResourceViewRange range) const
{
    if (old_state != new_state)
    {
        rg.m_passes.push_back([&image, old_ = old_state, new_ = new_state, range](CommandBuffer& _command) {
            _command.transition_resource(image, old_, new_, range);
        });
    }
}

void RenderGraphBuilder::add_copy_to_image_pass(
    RenderGraph& rg,
    std::shared_ptr<BufferResource> staging,
    std::shared_ptr<ImageResource> image)
{
    rg.m_passes.push_back([staging, image](CommandBuffer& _command) {
        _command.transition_resource(*image, ImageResourceState::Undefined, ImageResourceState::TransferDst);
        _command.copy_buffer_to_image(*staging, *image);
    });
}

void RenderGraphBuilder::add_copy_to_buffer_pass(
    RenderGraph& rg,
    std::shared_ptr<BufferResource> staging,
    std::shared_ptr<BufferResource> buffer)
{
    rg.m_passes.push_back(
        [staging, buffer](CommandBuffer& _command) { _command.copy_buffer_to_buffer(*staging, *buffer); });
}

void RenderGraphBuilder::add_pass(
    RenderGraph& rg,
    const std::string& name,
    const RGPassResources& resources,
    const RGFunction& func) const
{
    rg.m_passes.push_back([name, resources, func](CommandBuffer& command) {
        // clang-format off
        if (!name.empty()) command.begin_gpu_marker(name);
        {
            func(command, resources);
        }
        if (!name.empty()) command.end_gpu_marker();
        // clang-format on
    });
}

std::vector<RenderGraphBuilder::RGImageUsage> RenderGraphBuilder::get_image_usages(RGImageRef ref) const
{
    MIZU_PROFILE_SCOPED;

    std::vector<RGImageUsage> usages;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        const RGBuilderPass& pass = m_passes[i];

        RGImageViewRef referenced_by_image_view = RGImageViewRef::invalid();
        ShaderParameterMemberInfo member_info;

        for (const ShaderParameterMemberInfo& info : pass.get_image_view_members())
        {
            const RGImageViewRef& view_ref = std::get<RGImageViewRef>(info.value);

            if (image_view_references_image(view_ref, ref))
            {
                referenced_by_image_view = view_ref;
                member_info = info;
                break;
            }
        }

        if (referenced_by_image_view != RGImageViewRef::invalid())
        {
            RGImageUsage usage{};
            usage.render_pass_idx = i;
            usage.image = ref;
            usage.view = referenced_by_image_view;

            if (member_info.mem_type == ShaderParameterMemberType::RGSampledImageView)
            {
                usage.type = RGImageUsage::Type::Sampled;
            }
            else if (member_info.mem_type == ShaderParameterMemberType::RGStorageImageView)
            {
                usage.type = RGImageUsage::Type::Storage;
            }
            else
            {
                MIZU_UNREACHABLE("Invalid member type");
            }

            usages.push_back(usage);
        }
        else if (pass.has_framebuffer())
        {
            const RGFramebufferAttachments& framebuffer_desc = pass.get_framebuffer();

            RGImageUsage usage{};
            usage.type = RGImageUsage::Type::Attachment;
            usage.render_pass_idx = i;
            usage.image = ref;

            if (framebuffer_desc.depth_stencil_attachment != RGImageViewRef::invalid()
                && image_view_references_image(framebuffer_desc.depth_stencil_attachment, ref))
            {
                usage.view = framebuffer_desc.depth_stencil_attachment;
                usages.push_back(usage);

                continue;
            }

            for (const RGImageViewRef attachment_view : framebuffer_desc.color_attachments)
            {
                if (image_view_references_image(attachment_view, ref))
                {
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
    MIZU_PROFILE_SCOPED;

    std::vector<RGBufferUsage> usages;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        const RGBuilderPass& pass = m_passes[i];

        for (const ShaderParameterMemberInfo& info : pass.get_buffer_members())
        {
            RGBufferUsage usage{};
            usage.render_pass_idx = i;

            if (info.mem_type == ShaderParameterMemberType::RGUniformBuffer
                && std::get<RGUniformBufferRef>(info.value) == ref)
            {
                usage.type = RGBufferUsage::Type::UniformBuffer;
                usages.push_back(usage);
            }
            else if (
                info.mem_type == ShaderParameterMemberType::RGStorageBuffer
                && std::get<RGStorageBufferRef>(info.value) == ref)
            {
                usage.type = RGBufferUsage::Type::StorageBuffer;
                usages.push_back(usage);
            }
        }
    }

    return usages;
}

Framebuffer::Attachment RenderGraphBuilder::create_framebuffer_attachment(
    RenderGraph& rg,
    const RGImageViewRef& attachment_view,
    const RGImageMap& image_resources,
    const RGImageViewMap& image_view_resources,
    const std::unordered_map<RGImageRef, std::vector<RGImageUsage>>& image_usages,
    size_t pass_idx)
{
    MIZU_PROFILE_SCOPED;

    // TODO: This could cause problems, if we end up adding external image views to the RenderGraph
    MIZU_ASSERT(
        m_transient_image_view_descriptions.contains(attachment_view),
        "Image view with id {} not registered as resource",
        static_cast<UUID::Type>(attachment_view));
    const RGImageViewDescription& desc = m_transient_image_view_descriptions.find(attachment_view)->second;

    MIZU_ASSERT(
        image_resources.contains(desc.image_ref),
        "Image view with id {} not registered as resource",
        static_cast<UUID::Type>(desc.image_ref));
    const std::shared_ptr<ImageResource>& image = image_resources.find(desc.image_ref)->second;

    MIZU_ASSERT(
        image_view_resources.contains(attachment_view),
        "Image view with id {} not registered as resource",
        static_cast<UUID::Type>(attachment_view));
    const std::shared_ptr<ImageResourceView>& image_view = image_view_resources.find(attachment_view)->second;

    MIZU_ASSERT(
        image_usages.contains(desc.image_ref),
        "Image with id {} is not registered in image_usages",
        static_cast<UUID::Type>(desc.image_ref));
    const std::vector<RGImageUsage>& usages = image_usages.find(desc.image_ref)->second;

    const auto& it_usages =
        std::ranges::find_if(usages, [pass_idx](RGImageUsage usage) { return usage.render_pass_idx == pass_idx; });
    MIZU_ASSERT(it_usages != usages.end(), "If texture is attachment of a framebuffer, should have at least one usage");

    const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

    const bool image_is_external =
        std::ranges::find_if(
            m_external_image_descriptions,
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

        const ImageResourceViewRange range = image_view_resources.find(attachment_view)->second->get_range();
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
    attachment.image_view = image_view_resources.find(attachment_view)->second;
    attachment.load_operation = load_operation;
    attachment.store_operation = store_operation;
    attachment.initial_state = ImageUtils::is_depth_format(image->get_format())
                                   ? ImageResourceState::DepthStencilAttachment
                                   : ImageResourceState::ColorAttachment;
    attachment.final_state = attachment.initial_state;
    attachment.clear_value = clear_value;

    // Prepare image for attachment usage
    add_image_transition_pass(rg, *image, initial_state, final_state, image_view->get_range());

    return attachment;
}

} // namespace Mizu
