#include "render/render_graph/render_graph_builder.h"

#include <algorithm>
#include <cstring>
#include <format>

#include "base/debug/profiling.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/framebuffer.h"
#include "render_core/rhi/resource_group.h"
#include "render_core/rhi/sampler_state.h"

#include "render/render_graph/render_graph_resource_aliasing.h"
#include "render/renderer.h"

namespace Mizu
{

RGImageRef RenderGraphBuilder::create_image(ImageDescription desc, std::vector<uint8_t> data)
{
    RGImageDescription image_desc{};
    image_desc.image_desc = desc;
    image_desc.data = std::move(data);

    const auto id = RGImageRef{};
    m_transient_image_descriptions.insert({id, image_desc});

    return id;
}

RGImageRef RenderGraphBuilder::create_image(ImageDescription desc)
{
    return create_image(desc, std::vector<uint8_t>{});
}

RGImageRef RenderGraphBuilder::create_cubemap(glm::uvec2 dimensions, ImageFormat format, std::string name)
{
    ImageDescription cubemap_desc{};
    cubemap_desc.width = dimensions.x;
    cubemap_desc.height = dimensions.y;
    cubemap_desc.type = ImageType::Cubemap;
    cubemap_desc.format = format;
    cubemap_desc.name = std::move(name);

    RGImageDescription desc{};
    desc.image_desc = cubemap_desc;

    auto id = RGImageRef();
    m_transient_image_descriptions.insert({id, desc});

    return id;
}

RGBufferRef RenderGraphBuilder::create_buffer(BufferDescription desc, std::vector<uint8_t> data)
{
    RGBufferDescription buffer_desc{};
    buffer_desc.buffer_desc = desc;
    buffer_desc.data = std::move(data);

    if (buffer_desc.buffer_desc.size == 0)
    {
        // Cannot create empty buffer, if size is 0 set to 1
        // TODO: Rethink this approach
        buffer_desc.buffer_desc.size = 1;
    }

    const auto id = RGBufferRef{};
    m_transient_buffer_descriptions.insert({id, buffer_desc});

    return id;
}

RGBufferRef RenderGraphBuilder::create_buffer(BufferDescription desc)
{
    return create_buffer(desc, std::vector<uint8_t>{});
}

RGBufferRef RenderGraphBuilder::create_buffer(size_t size, size_t stride, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = stride;
    desc.name = std::move(name);

    return create_buffer(desc);
}

RGBufferRef RenderGraphBuilder::register_external_constant_buffer(
    std::shared_ptr<BufferResource> cbo,
    RGExternalBufferParams params)
{
    RGExternalBufferDescription desc{};
    desc.resource = cbo;
    desc.input_state = params.input_state;
    desc.output_state = params.output_state;

    auto id = RGBufferRef();
    m_external_buffer_descriptions.insert({id, desc});

    return id;
}

RGBufferRef RenderGraphBuilder::register_external_structured_buffer(
    std::shared_ptr<BufferResource> sbo,
    RGExternalBufferParams params)
{
    RGExternalBufferDescription desc{};
    desc.resource = sbo;
    desc.input_state = params.input_state;
    desc.output_state = params.output_state;

    auto id = RGBufferRef();
    m_external_buffer_descriptions.insert({id, desc});

    return id;
}

RGAccelerationStructureRef RenderGraphBuilder::register_external_acceleration_structure(
    std::shared_ptr<AccelerationStructure> as)
{
    auto id = RGAccelerationStructureRef{};
    m_external_as.insert({id, as});

    return id;
}

RGResourceGroupRef RenderGraphBuilder::create_resource_group(const RGResourceGroupLayout& layout)
{
    const size_t hash = layout.hash();

    auto it = m_resource_group_cache.find(hash);
    if (it != m_resource_group_cache.end())
        return it->second;

    auto id = RGResourceGroupRef{};
    m_resource_group_descriptions.insert({id, layout});
    m_resource_group_cache.insert({hash, id});

    return id;
}

RGTextureSrvRef RenderGraphBuilder::create_texture_srv(
    RGImageRef image_ref,
    ImageFormat format,
    ImageResourceViewRange range)
{
    RGTextureViewDescription view_desc{};
    view_desc.image_ref = image_ref;
    view_desc.usage = RGResourceUsageType::Read;
    view_desc.format = format;
    view_desc.range = range;

    const auto id = RGTextureSrvRef{};
    m_transient_texture_view_descriptions.insert({id, view_desc});

    return id;
}

RGTextureSrvRef RenderGraphBuilder::create_texture_srv(RGImageRef image_ref, ImageResourceViewRange range)
{
    const ImageFormat format = get_image_format(image_ref);
    return create_texture_srv(image_ref, format, range);
}

RGTextureUavRef RenderGraphBuilder::create_texture_uav(
    RGImageRef image_ref,
    ImageFormat format,
    ImageResourceViewRange range)
{
    RGTextureViewDescription view_desc{};
    view_desc.image_ref = image_ref;
    view_desc.usage = RGResourceUsageType::ReadWrite;
    view_desc.format = format;
    view_desc.range = range;

    const auto id = RGTextureUavRef{};
    m_transient_texture_view_descriptions.insert({id, view_desc});

    return id;
}

RGTextureUavRef RenderGraphBuilder::create_texture_uav(RGImageRef image_ref, ImageResourceViewRange range)
{
    const ImageFormat format = get_image_format(image_ref);
    return create_texture_uav(image_ref, format, range);
}

RGTextureRtvRef RenderGraphBuilder::create_texture_rtv(
    RGImageRef image_ref,
    ImageFormat format,
    ImageResourceViewRange range)
{
    RGTextureViewDescription view_desc{};
    view_desc.image_ref = image_ref;
    view_desc.usage = RGResourceUsageType::Attachment;
    view_desc.format = format;
    view_desc.range = range;

    const auto id = RGTextureRtvRef{};
    m_transient_texture_view_descriptions.insert({id, view_desc});

    return id;
}

RGTextureRtvRef RenderGraphBuilder::create_texture_rtv(RGImageRef image_ref, ImageResourceViewRange range)
{
    const ImageFormat format = get_image_format(image_ref);
    return create_texture_rtv(image_ref, format, range);
}

RGBufferSrvRef RenderGraphBuilder::create_buffer_srv(RGBufferRef buffer_ref)
{
    RGBufferViewDescription view_desc{};
    view_desc.buffer_ref = buffer_ref;
    view_desc.usage = RGResourceUsageType::Read;

    const auto id = RGBufferSrvRef{};
    m_transient_buffer_view_descriptions.insert({id, view_desc});

    return id;
}

RGBufferSrvRef RenderGraphBuilder::create_buffer_uav(RGBufferRef buffer_ref)
{
    RGBufferViewDescription view_desc{};
    view_desc.buffer_ref = buffer_ref;
    view_desc.usage = RGResourceUsageType::ReadWrite;

    const auto id = RGBufferSrvRef{};
    m_transient_buffer_view_descriptions.insert({id, view_desc});

    return id;
}

RGBufferCbvRef RenderGraphBuilder::create_buffer_cbv(RGBufferRef buffer_ref)
{
    RGBufferViewDescription view_desc{};
    view_desc.buffer_ref = buffer_ref;
    view_desc.usage = RGResourceUsageType::ConstantBuffer;

    const auto id = RGBufferCbvRef{};
    m_transient_buffer_view_descriptions.insert({id, view_desc});

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

//
// Compilation
//

#define MIZU_RG_INIT_BUFFERS_WITH_STAGING_BUFFER 0

void RenderGraphBuilder::compile(RenderGraph& rg, const RenderGraphBuilderMemory& memory)
{
    MIZU_PROFILE_SCOPED;

    rg.reset();

    AliasedDeviceMemoryAllocator& allocator = memory.transient_allocator;
    AliasedDeviceMemoryAllocator& host_allocator = memory.host_allocator;

    // 1. Compute total size of transient resources
    // 1.1. Get usage of resources, to check dependencies and if some resources can overlap
    // 1.2. Compute total size taking into account possible aliasing

    std::vector<RGResourceLifetime> resources;
    std::vector<RGResourceLifetime> host_resources;

    std::unordered_map<RGResourceRef, std::vector<RGResourceUsage>> resource_usages;

    RGImageMap image_resources;
    std::unordered_map<RGImageRef, std::shared_ptr<BufferResource>> image_to_staging_buffer;

    for (const auto& [id, desc] : m_transient_image_descriptions)
    {
        get_image_usages(id, resource_usages);

        const std::vector<RGResourceUsage>& usages = resource_usages[id];
        if (usages.empty())
        {
            MIZU_LOG_WARNING("Ignoring image with id {} because no usage was found", static_cast<UUID::Type>(id));
            continue;
        }

        ImageUsageBits usage_bits = ImageUsageBits::None;
        for (const RGResourceUsage& usage : usages)
        {
            ImageUsageBits image_usage = ImageUsageBits::None;
            switch (usage.usage)
            {
            case RGResourceUsageType::Attachment:
                image_usage = ImageUsageBits::Attachment;
                break;
            case RGResourceUsageType::Read:
                image_usage = ImageUsageBits::Sampled;
                break;
            case RGResourceUsageType::ReadWrite:
                image_usage = ImageUsageBits::UnorderedAccess;
                break;
            default:
                MIZU_UNREACHABLE("Invalid RGResourceUsageType for image");
            }

            usage_bits |= image_usage;
        }

        ImageDescription transient_desc = desc.image_desc;
        transient_desc.usage = usage_bits;
        transient_desc.is_virtual = true;

        if (!desc.data.empty())
        {
            transient_desc.usage |= ImageUsageBits::TransferDst;
        }

        const auto transient = g_render_device->create_image(transient_desc);
        image_resources.insert({id, transient});

        if (!desc.data.empty())
        {
            const std::string staging_name = std::format("Staging_{}", desc.image_desc.name);

            BufferDescription staging_desc{};
            staging_desc.size = desc.data.size();
            staging_desc.usage = BufferUsageBits::TransferSrc | BufferUsageBits::HostVisible;
            staging_desc.is_virtual = true;

            const auto staging_buffer = g_render_device->create_buffer(staging_desc);
            image_to_staging_buffer.insert({id, staging_buffer});

            const MemoryRequirements staging_reqs = staging_buffer->get_memory_requirements();

            RGResourceLifetime staging_lifetime{};
            staging_lifetime.begin = 0;
            staging_lifetime.end = m_passes.size() - 1;
            staging_lifetime.size = staging_reqs.size;
            staging_lifetime.alignment = staging_reqs.alignment;
            staging_lifetime.value = id;
            staging_lifetime.transient_buffer = staging_buffer;

            host_resources.push_back(staging_lifetime);
        }

        const MemoryRequirements reqs = transient->get_memory_requirements();

        RGResourceLifetime lifetime{};
        lifetime.begin = usages[0].pass_idx;
        lifetime.end = usages[usages.size() - 1].pass_idx;
        lifetime.size = reqs.size;
        lifetime.alignment = reqs.alignment;
        lifetime.value = id;
        lifetime.transient_image = transient;

        resources.push_back(lifetime);
    }

    RGBufferMap buffer_resources;
    std::unordered_map<RGBufferRef, std::shared_ptr<BufferResource>> buffer_to_staging_buffer;

    for (const auto& [id, desc] : m_transient_buffer_descriptions)
    {
        get_buffer_usages(id, resource_usages);

        const std::vector<RGResourceUsage>& usages = resource_usages[id];
        if (usages.empty())
        {
            MIZU_LOG_WARNING("Ignoring buffer with id {} because no usage was found", static_cast<UUID::Type>(id));
            continue;
        }

        BufferUsageBits usage_bits = BufferUsageBits::None;
        for (const RGResourceUsage& usage : usages)
        {
            BufferUsageBits buffer_usage = BufferUsageBits::None;
            switch (usage.usage)
            {
            case RGResourceUsageType::Read:
                break;
            case RGResourceUsageType::ReadWrite:
                buffer_usage = BufferUsageBits::UnorderedAccess;
                break;
            case RGResourceUsageType::ConstantBuffer:
                buffer_usage = BufferUsageBits::ConstantBuffer;
                break;
            default:
                MIZU_UNREACHABLE("Invalid RGResourceUsageType for image");
            }

            usage_bits |= buffer_usage;
        }

        if (!desc.data.empty())
        {
#if MIZU_RG_INIT_BUFFERS_WITH_STAGING_BUFFER
            usage_bits |= BufferUsageBits::TransferDst;
#else
            usage_bits |= BufferUsageBits::HostVisible;
#endif
        }

        BufferDescription transient_desc = desc.buffer_desc;
        transient_desc.usage |= usage_bits;
        transient_desc.is_virtual = true;

        const auto transient = g_render_device->create_buffer(transient_desc);
        buffer_resources.insert({id, transient});

        if (!desc.data.empty())
        {
            RGResourceLifetime staging_lifetime{};
            staging_lifetime.begin = 0;
            staging_lifetime.end = m_passes.size() - 1;
            staging_lifetime.value = id;

#if MIZU_RG_INIT_BUFFERS_WITH_STAGING_BUFFER
            const std::string staging_name = std::format("Staging_{}", desc.buffer_desc.name);

            BufferDescription staging_desc = StagingBuffer::get_buffer_description(transient_desc.size, staging_name);
            staging_desc.is_virtual = true;

            const auto staging_buffer = BufferResource::create(staging_desc);
            buffer_to_staging_buffer.insert({id, staging_buffer});

            const MemoryRequirements staging_reqs = staging_buffer->get_memory_requirements();

            staging_lifetime.size = staging_reqs.size;
            staging_lifetime.alignment = staging_reqs.alignment;
            staging_lifetime.transient_buffer = staging_buffer;
#else
            const MemoryRequirements transient_reqs = transient->get_memory_requirements();

            staging_lifetime.size = transient_reqs.size;
            staging_lifetime.alignment = transient_reqs.alignment;
            staging_lifetime.transient_buffer = transient;
#endif

            host_resources.push_back(staging_lifetime);
        }

#if !MIZU_RG_INIT_BUFFERS_WITH_STAGING_BUFFER
        if (desc.data.empty())
#endif
        {
            const MemoryRequirements reqs = transient->get_memory_requirements();

            RGResourceLifetime lifetime{};
            lifetime.begin = usages[0].pass_idx;
            lifetime.end = usages[usages.size() - 1].pass_idx;
            lifetime.size = reqs.size;
            lifetime.alignment = reqs.alignment;
            lifetime.value = id;
            lifetime.transient_buffer = transient;

            resources.push_back(lifetime);
        }
    }

    RGAccelerationStructureMap acceleration_structure_resources;

    // 2. Allocate resources using aliasing

    // 2.1 Allocate transient resources
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
        size <= allocator.get_allocated_size(),
        "Expected size and allocated size do not match ({} != {})",
        size,
        allocator.get_allocated_size());

    // 2.2 Allocate staging resources
    [[maybe_unused]] const size_t staging_size = alias_resources(host_resources);

    for (const RGResourceLifetime& resource : host_resources)
    {
        MIZU_ASSERT(
            std::holds_alternative<RGBufferRef>(resource.value) && resource.transient_buffer != nullptr,
            "Staging resources can only be buffers");

        host_allocator.allocate_buffer_resource(*resource.transient_buffer, resource.offset);
    }

    host_allocator.allocate();
    MIZU_ASSERT(
        staging_size <= host_allocator.get_allocated_size(),
        "Expected size and allocated size do not match ({} != {})",
        staging_size,
        host_allocator.get_allocated_size());

    if (!host_resources.empty())
    {
        uint8_t* mapped_staging = host_allocator.get_mapped_memory();

        for (const RGResourceLifetime& resource : host_resources)
        {
            const auto it = m_transient_buffer_descriptions.find(std::get<RGBufferRef>(resource.value));
            MIZU_ASSERT(
                it != m_transient_buffer_descriptions.end(),
                "Staging resource must have a corresponding transient buffer");

            const uint8_t* data = it->second.data.data();
            memcpy(mapped_staging + resource.offset, data, resource.size);
        }
    }

    // 3. Add external resources
    for (const auto& [id, desc] : m_external_image_descriptions)
    {
        image_resources.insert({id, desc.resource});
        get_image_usages(id, resource_usages);
    }

    for (const auto& [id, buffer] : m_external_buffer_descriptions)
    {
        buffer_resources.insert({id, buffer.resource});
        get_buffer_usages(id, resource_usages);
    }

    for (const auto& [id, as] : m_external_as)
    {
        acceleration_structure_resources.insert({id, as});
    }

    // 4. Create resource views
    RGTextureViewsWrapper texture_views;

    for (const auto& [id, desc] : m_transient_texture_view_descriptions)
    {
        const auto it = image_resources.find(desc.image_ref);
        MIZU_ASSERT(
            it != image_resources.end(),
            "Image view with id {} references not existent image with id {}",
            static_cast<UUID::Type>(id),
            static_cast<UUID::Type>(desc.image_ref));

        if (desc.usage == RGResourceUsageType::Read)
        {
            const auto srv = g_render_device->create_srv(it->second, desc.range);
            texture_views.add(static_cast<RGTextureSrvRef>(id), srv);
        }
        else if (desc.usage == RGResourceUsageType::ReadWrite)
        {
            const auto uav = g_render_device->create_uav(it->second, desc.range);
            texture_views.add(static_cast<RGTextureUavRef>(id), uav);
        }
        else if (desc.usage == RGResourceUsageType::Attachment)
        {
            const auto rtv = g_render_device->create_rtv(it->second, desc.range);
            texture_views.add(static_cast<RGTextureRtvRef>(id), rtv);
        }
        else
        {
            MIZU_UNREACHABLE("Invalid RGResourceUsageType for image view");
        }
    }

    RGBufferViewsWrapper buffer_views;

    for (const auto& [id, desc] : m_transient_buffer_view_descriptions)
    {
        const auto it = buffer_resources.find(desc.buffer_ref);
        MIZU_ASSERT(
            it != buffer_resources.end(),
            "Image view with id {} references not existent image with id {}",
            static_cast<UUID::Type>(id),
            static_cast<UUID::Type>(desc.buffer_ref));

        if (desc.usage == RGResourceUsageType::Read)
        {
            const auto srv = g_render_device->create_srv(it->second);
            buffer_views.add(static_cast<RGBufferSrvRef>(id), srv);
        }
        else if (desc.usage == RGResourceUsageType::ReadWrite)
        {
            const auto uav = g_render_device->create_uav(it->second);
            buffer_views.add(static_cast<RGBufferUavRef>(id), uav);
        }
        else if (desc.usage == RGResourceUsageType::ConstantBuffer)
        {
            const auto cbv = g_render_device->create_cbv(it->second);
            buffer_views.add(static_cast<RGBufferCbvRef>(id), cbv);
        }
        else
        {
            MIZU_UNREACHABLE("Invalid RGResourceUsageType for buffer view");
        }
    }

    // 5. Create resource groups
    RGResourceGroupMap& resource_group_resources = rg.m_resource_group_map;

    for (const auto& [id, desc] : m_resource_group_descriptions)
    {
        ResourceGroupBuilder builder;

        for (const RGResourceGroupLayoutResource& resource : desc.get_resources())
        {
            ResourceGroupItem item{};

            if (resource.is_type<RGLayoutResourceTextureSrv>())
            {
                const RGLayoutResourceTextureSrv& value = resource.as_type<RGLayoutResourceTextureSrv>();
                const std::shared_ptr<ShaderResourceView>& view = texture_views.get(value.value);

                item = ResourceGroupItem::TextureSrv(resource.binding, view, resource.stage);
            }
            else if (resource.is_type<RGLayoutResourceTextureUav>())
            {
                const RGLayoutResourceTextureUav& value = resource.as_type<RGLayoutResourceTextureUav>();
                const std::shared_ptr<UnorderedAccessView>& view = texture_views.get(value.value);

                item = ResourceGroupItem::TextureUav(resource.binding, view, resource.stage);
            }
            else if (resource.is_type<RGLayoutResourceBufferSrv>())
            {
                const RGLayoutResourceBufferSrv& value = resource.as_type<RGLayoutResourceBufferSrv>();
                const std::shared_ptr<ShaderResourceView>& view = buffer_views.get(value.value);

                item = ResourceGroupItem::BufferSrv(resource.binding, view, resource.stage);
            }
            else if (resource.is_type<RGLayoutResourceBufferUav>())
            {
                const RGLayoutResourceBufferUav& value = resource.as_type<RGLayoutResourceBufferUav>();
                const std::shared_ptr<UnorderedAccessView>& view = buffer_views.get(value.value);

                item = ResourceGroupItem::BufferUav(resource.binding, view, resource.stage);
            }
            else if (resource.is_type<RGLayoutResourceBufferCbv>())
            {
                const RGLayoutResourceBufferCbv& value = resource.as_type<RGLayoutResourceBufferCbv>();
                const std::shared_ptr<ConstantBufferView>& view = buffer_views.get(value.value);

                item = ResourceGroupItem::ConstantBuffer(resource.binding, view, resource.stage);
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

        const auto resource_group = g_render_device->create_resource_group(builder);
        resource_group_resources.insert({id, resource_group});
    }

    // 6. Create passes to execute
    for (size_t pass_idx = 0; pass_idx < m_passes.size(); ++pass_idx)
    {
        const RGBuilderPass& pass = m_passes[pass_idx];

        // 6.1 Create pass resources container
        RGPassResources pass_resources;
        pass_resources.set_resource_group_map(&resource_group_resources);

        for (const ShaderParameterMemberInfo& info : pass.get_texture_view_members())
        {
            RGTextureViewRef texture_view_ref;
            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGTextureSrv: {
                const RGTextureSrvRef view_ref = std::get<RGTextureSrvRef>(info.value);
                pass_resources.add_texture_srv(view_ref, texture_views.get(view_ref));

                texture_view_ref = view_ref;
                break;
            }
            case ShaderParameterMemberType::RGTextureUav: {
                const RGTextureUavRef view_ref = std::get<RGTextureUavRef>(info.value);
                pass_resources.add_texture_uav(view_ref, texture_views.get(view_ref));

                texture_view_ref = view_ref;
                break;
            }
            default:
                MIZU_UNREACHABLE("Invalid ShaderParameterMemberType for texture view");
            }

            const RGImageRef image_ref = get_image_from_texture_view(texture_view_ref);
            pass_resources.add_image(image_ref, image_resources[image_ref]);
        }

        if (pass.has_framebuffer())
        {
            const RGFramebufferAttachments& attachments = pass.get_framebuffer();

            for (const RGTextureRtvRef& rtv_ref : attachments.color_attachments)
            {
                pass_resources.add_texture_rtv(rtv_ref, texture_views.get(rtv_ref));

                const RGImageRef image_ref = get_image_from_texture_view(rtv_ref);
                pass_resources.add_image(image_ref, image_resources[image_ref]);
            }

            if (attachments.depth_stencil_attachment != RGTextureRtvRef::invalid())
            {
                pass_resources.add_texture_rtv(
                    attachments.depth_stencil_attachment, texture_views.get(attachments.depth_stencil_attachment));

                const RGImageRef image_ref = get_image_from_texture_view(attachments.depth_stencil_attachment);
                pass_resources.add_image(image_ref, image_resources[image_ref]);
            }
        }

        for (const ShaderParameterMemberInfo& info : pass.get_buffer_view_members())
        {
            RGBufferViewRef buffer_view_ref;
            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGBufferSrv: {
                const RGBufferSrvRef view_ref = std::get<RGBufferSrvRef>(info.value);
                pass_resources.add_buffer_srv(view_ref, buffer_views.get(view_ref));

                buffer_view_ref = view_ref;
                break;
            }
            case ShaderParameterMemberType::RGBufferUav: {
                const RGBufferUavRef view_ref = std::get<RGBufferUavRef>(info.value);
                pass_resources.add_buffer_uav(view_ref, buffer_views.get(view_ref));

                buffer_view_ref = view_ref;
                break;
            }
            case ShaderParameterMemberType::RGBufferCbv: {
                const RGBufferCbvRef view_ref = std::get<RGBufferCbvRef>(info.value);
                pass_resources.add_buffer_cbv(view_ref, buffer_views.get(view_ref));

                buffer_view_ref = view_ref;
                break;
            }
            default:
                MIZU_UNREACHABLE("Invalid ShaderParameterMemberType for buffer view");
            }

            const RGBufferRef buffer_ref = get_buffer_from_buffer_view(buffer_view_ref);
            pass_resources.add_buffer(buffer_ref, buffer_resources[buffer_ref]);
        }

        // 6.2. If RGPassHint is Immediate, just execute the function
        if (pass.get_hint() == RGPassHint::Immediate)
        {
            add_pass(rg, pass.get_name(), pass_resources, pass.get_function());

            continue;
        }

        // 6.3. Transfer staging buffers to resources if it's first usage
        for (const ShaderParameterMemberInfo& info : pass.get_texture_view_members())
        {
            RGTextureViewRef view_ref;
            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGTextureSrv:
                view_ref = std::get<RGTextureSrvRef>(info.value);
                break;
            case ShaderParameterMemberType::RGTextureUav:
                view_ref = std::get<RGTextureUavRef>(info.value);
                break;
            default:
                MIZU_UNREACHABLE("Invalid ShaderParameterMemberType for texture view");
            }

            const RGImageRef& image_ref = get_image_from_texture_view(view_ref);

            const std::vector<RGResourceUsage>& usages = resource_usages[image_ref];
            if (usages[0].pass_idx != pass_idx)
            {
                continue;
            }

            const auto it = image_to_staging_buffer.find(image_ref);
            if (it == image_to_staging_buffer.end())
            {
                continue;
            }

            const auto& staging_buffer = it->second;
            // Add staging buffer to pass resources for lifetime management
            pass_resources.add_buffer(RGBufferRef(), staging_buffer);

            add_buffer_transition_pass(
                rg, *staging_buffer, BufferResourceState::Undefined, BufferResourceState::TransferSrc);
            add_copy_to_image_pass(rg, *staging_buffer, *image_resources[image_ref]);
        }

        for (const ShaderParameterMemberInfo& info : pass.get_buffer_view_members())
        {
            RGBufferViewRef view_ref;
            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGBufferSrv:
                view_ref = std::get<RGBufferSrvRef>(info.value);
                break;
            case ShaderParameterMemberType::RGBufferUav:
                view_ref = std::get<RGBufferUavRef>(info.value);
                break;
            case ShaderParameterMemberType::RGBufferCbv:
                view_ref = std::get<RGBufferCbvRef>(info.value);
                break;
            default:
                MIZU_UNREACHABLE("Invalid ShaderParameterMemberType for buffer view");
            }

            const RGBufferRef& buffer_ref = get_buffer_from_buffer_view(view_ref);

            const std::vector<RGResourceUsage>& usages = resource_usages[buffer_ref];
            if (usages[0].pass_idx != pass_idx)
            {
                continue;
            }

            const auto it = buffer_to_staging_buffer.find(buffer_ref);
            if (it == buffer_to_staging_buffer.end())
            {
                continue;
            }

            const auto& staging_buffer = it->second;
            // Add staging buffer to pass resources for lifetime management
            pass_resources.add_buffer(RGBufferRef(), staging_buffer);

            add_buffer_transition_pass(
                rg, *staging_buffer, BufferResourceState::Undefined, BufferResourceState::TransferSrc);
            add_copy_to_buffer_pass(rg, *staging_buffer, *buffer_resources[buffer_ref]);
        }

        // 6.4. Transition pass dependencies
        for (const ShaderParameterMemberInfo& info : pass.get_texture_view_members())
        {
            RGTextureViewRef view_ref;
            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGTextureSrv:
                view_ref = std::get<RGTextureSrvRef>(info.value);
                break;
            case ShaderParameterMemberType::RGTextureUav:
                view_ref = std::get<RGTextureUavRef>(info.value);
                break;
            default:
                MIZU_UNREACHABLE("Invalid ShaderParameterMemberType for texture view");
            }

            const RGImageRef& image_ref = get_image_from_texture_view(view_ref);

            const std::vector<RGResourceUsage>& usages = resource_usages[image_ref];
            const std::shared_ptr<ImageResource>& image = image_resources[image_ref];

            const auto& it_usages = std::find_if(
                usages.begin(), usages.end(), [pass_idx](RGResourceUsage usage) { return usage.pass_idx == pass_idx; });
            MIZU_ASSERT(
                it_usages != usages.end(),
                "No usage was found for image {} in the pass number {}",
                static_cast<UUID::Type>(image_ref),
                pass_idx);

            const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

            const bool is_image_external =
                std::find_if(
                    m_external_image_descriptions.begin(),
                    m_external_image_descriptions.end(),
                    [image_ref](const std::pair<RGImageRef, RGExternalImageDescription>& pair) {
                        return pair.first == image_ref;
                    })
                != m_external_image_descriptions.end();

            ImageResourceState initial_state = ImageResourceState::Undefined;
            if (usage_pos == 0)
            {
                if (is_image_external)
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
                const RGResourceUsage& previous_usage = usages[usage_pos - 1];

                switch (previous_usage.usage)
                {
                case RGResourceUsageType::Attachment:
                    initial_state = is_depth_format(image->get_format()) ? ImageResourceState::DepthStencilAttachment
                                                                         : ImageResourceState::ColorAttachment;
                    break;
                case RGResourceUsageType::Read:
                    initial_state = ImageResourceState::ShaderReadOnly;
                    break;
                case RGResourceUsageType::ReadWrite:
                    initial_state = ImageResourceState::UnorderedAccess;
                    break;
                case RGResourceUsageType::ConstantBuffer:
                    // Does not apply
                    break;
                }
            }

            const RGResourceUsage& usage = usages[usage_pos];
            ImageResourceState final_state = ImageResourceState::Undefined;

            switch (usage.usage)
            {
            case RGResourceUsageType::Attachment:
                MIZU_UNREACHABLE("If image is dependency, it can't have a usage of Attachment");
                break;
            case RGResourceUsageType::Read:
                final_state = ImageResourceState::ShaderReadOnly;
                break;
            case RGResourceUsageType::ReadWrite:
                final_state = ImageResourceState::UnorderedAccess;
                break;
            case RGResourceUsageType::ConstantBuffer:
                // Does not apply
                break;
            }

            add_image_transition_pass(rg, *image, initial_state, final_state);
        }

        for (const ShaderParameterMemberInfo& info : pass.get_buffer_view_members())
        {
            RGBufferViewRef view_ref;
            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGBufferSrv:
                view_ref = std::get<RGBufferSrvRef>(info.value);
                break;
            case ShaderParameterMemberType::RGBufferUav:
                view_ref = std::get<RGBufferUavRef>(info.value);
                break;
            case ShaderParameterMemberType::RGBufferCbv:
                view_ref = std::get<RGBufferCbvRef>(info.value);
                break;
            default:
                MIZU_UNREACHABLE("Invalid ShaderParameterMemberType for buffer view");
            }

            const RGBufferRef& buffer_ref = get_buffer_from_buffer_view(view_ref);

            const std::vector<RGResourceUsage>& usages = resource_usages[buffer_ref];
            const std::shared_ptr<BufferResource>& buffer = buffer_resources[buffer_ref];

            const auto& it_usages = std::find_if(
                usages.begin(), usages.end(), [pass_idx](RGResourceUsage usage) { return usage.pass_idx == pass_idx; });
            MIZU_ASSERT(
                it_usages != usages.end(),
                "No usage was found for buffer {} in the pass number {}",
                static_cast<UUID::Type>(buffer_ref),
                pass_idx);

            const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

            const bool is_buffer_external =
                std::find_if(
                    m_external_buffer_descriptions.begin(),
                    m_external_buffer_descriptions.end(),
                    [buffer_ref](const std::pair<RGBufferRef, RGExternalBufferDescription>& pair) {
                        return pair.first == buffer_ref;
                    })
                != m_external_buffer_descriptions.end();

            BufferResourceState initial_state = BufferResourceState::Undefined;
            if (usage_pos == 0)
            {
                if (is_buffer_external)
                {
                    const RGExternalBufferDescription& desc = m_external_buffer_descriptions[buffer_ref];
                    initial_state = desc.input_state;
                }
                else if (buffer->get_usage() & BufferUsageBits::TransferDst)
                {
                    initial_state = BufferResourceState::TransferDst;
                }
                else
                {
                    initial_state = BufferResourceState::Undefined;
                }
            }
            else
            {
                const RGResourceUsage& previous_usage = usages[usage_pos - 1];

                switch (previous_usage.usage)
                {
                case RGResourceUsageType::Read:
                case RGResourceUsageType::ConstantBuffer:
                    initial_state = BufferResourceState::ShaderReadOnly;
                    break;
                case RGResourceUsageType::ReadWrite:
                    initial_state = BufferResourceState::UnorderedAccess;
                    break;
                case RGResourceUsageType::Attachment:
                    // Does not apply
                    break;
                }
            }

            const RGResourceUsage& usage = usages[usage_pos];
            BufferResourceState final_state = BufferResourceState::Undefined;

            switch (usage.usage)
            {
            case RGResourceUsageType::Read:
            case RGResourceUsageType::ConstantBuffer:
                final_state = BufferResourceState::ShaderReadOnly;
                break;
            case RGResourceUsageType::ReadWrite:
                final_state = BufferResourceState::UnorderedAccess;
                break;
            case RGResourceUsageType::Attachment:
                // Does not apply
                break;
            }

            add_buffer_transition_pass(rg, *buffer, initial_state, final_state);
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

            FramebufferDescription framebuffer_desc{};
            framebuffer_desc.width = rg_framebuffer.width;
            framebuffer_desc.height = rg_framebuffer.height;

            for (const RGTextureRtvRef& attachment_view : rg_framebuffer.color_attachments)
            {
                framebuffer_desc.color_attachments.push_back(create_framebuffer_attachment(
                    rg, attachment_view, image_resources, texture_views, resource_usages, pass_idx));
            }

            if (rg_framebuffer.depth_stencil_attachment != RGTextureRtvRef::invalid())
            {
                framebuffer_desc.depth_stencil_attachment = create_framebuffer_attachment(
                    rg,
                    rg_framebuffer.depth_stencil_attachment,
                    image_resources,
                    texture_views,
                    resource_usages,
                    pass_idx);
            }

            const auto framebuffer = g_render_device->create_framebuffer(framebuffer_desc);
            pass_resources.set_framebuffer(framebuffer);
        }

        // 6.6 Create pass
        add_pass(rg, pass.get_name(), pass_resources, pass.get_function());
    }

    // 7. Transition external resources
    for (const auto& [id, desc] : m_external_image_descriptions)
    {
        const std::vector<RGResourceUsage>& usages = resource_usages[id];
        if (usages.empty())
        {
            MIZU_LOG_WARNING(
                "Ignoring external image transition for image with id {} because no usage was found",
                static_cast<UUID::Type>(id));
            continue;
        }

        ImageResourceState initial_state = ImageResourceState::Undefined;

        switch (usages[usages.size() - 1].usage)
        {
        case RGResourceUsageType::Attachment:
            initial_state = is_depth_format(desc.resource->get_format()) ? ImageResourceState::DepthStencilAttachment
                                                                         : ImageResourceState::ColorAttachment;
            break;
        case RGResourceUsageType::Read:
            initial_state = ImageResourceState::ShaderReadOnly;
            break;
        case RGResourceUsageType::ReadWrite:
            initial_state = ImageResourceState::UnorderedAccess;
            break;
        case RGResourceUsageType::ConstantBuffer:
            // Does not apply
            break;
        }

        add_image_transition_pass(rg, *desc.resource, initial_state, desc.output_state);
    }

    for (const auto& [id, desc] : m_external_buffer_descriptions)
    {
        const std::vector<RGResourceUsage>& usages = resource_usages[id];
        if (usages.empty())
        {
            MIZU_LOG_WARNING(
                "Ignoring external buffer transition for image with id {} because no usage was found",
                static_cast<UUID::Type>(id));
            continue;
        }

        BufferResourceState initial_state = BufferResourceState::Undefined;

        switch (usages[usages.size() - 1].usage)
        {
        case RGResourceUsageType::Read:
        case RGResourceUsageType::ConstantBuffer:
            initial_state = BufferResourceState::ShaderReadOnly;
            break;
        case RGResourceUsageType::ReadWrite:
            initial_state = BufferResourceState::UnorderedAccess;
            break;
        case RGResourceUsageType::Attachment:
            // Does not apply
            break;
        }

        add_buffer_transition_pass(rg, *desc.resource, initial_state, desc.output_state);
    }
}

//
// Helpers
//

void RenderGraphBuilder::get_image_usages(RGImageRef ref, RGResourceUsageMap& usages_map) const
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(
        !usages_map.contains(ref),
        "Usage of image resource with id {} has already been requested",
        static_cast<UUID::Type>(ref));

    std::vector<RGResourceUsage>& usages = usages_map.try_emplace(ref).first->second;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        const RGBuilderPass& pass = m_passes[i];

        RGResourceUsage usage{};
        usage.resource = ref;
        usage.pass_idx = i;
        usage.resource_type = RGResourceType::Image;

        for (const ShaderParameterMemberInfo& info : pass.get_texture_view_members())
        {
            RGTextureViewRef view_ref = RGTextureViewRef::invalid();

            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGTextureSrv: {
                view_ref = std::get<RGTextureSrvRef>(info.value);
                usage.usage = RGResourceUsageType::Read;
                break;
            }
            case ShaderParameterMemberType::RGTextureUav: {
                view_ref = std::get<RGTextureUavRef>(info.value);
                usage.usage = RGResourceUsageType::ReadWrite;
                break;
            }
            default:
                MIZU_UNREACHABLE("Invalid texture view type");
            }

            MIZU_ASSERT(view_ref != RGTextureViewRef::invalid(), "View ref does not have a valid value");

            if (texture_view_references_image(view_ref, ref))
            {
                usages.push_back(usage);
            }
        }

        if (pass.has_framebuffer())
        {
            const RGFramebufferAttachments& framebuffer_desc = pass.get_framebuffer();

            if (framebuffer_desc.depth_stencil_attachment != RGTextureViewRef::invalid()
                && texture_view_references_image(framebuffer_desc.depth_stencil_attachment, ref))
            {
                usage.usage = RGResourceUsageType::Attachment;
                usages.push_back(usage);

                continue;
            }

            for (const RGTextureViewRef& attachment_view : framebuffer_desc.color_attachments)
            {
                if (texture_view_references_image(attachment_view, ref))
                {
                    usage.usage = RGResourceUsageType::Attachment;
                    usages.push_back(usage);
                }
            }
        }
    }
}

void RenderGraphBuilder::get_buffer_usages(RGBufferRef ref, RGResourceUsageMap& usages_map) const
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(
        !usages_map.contains(ref),
        "Usage of image resource with id {} has already been requested",
        static_cast<UUID::Type>(ref));

    std::vector<RGResourceUsage>& usages = usages_map.try_emplace(ref).first->second;

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        const RGBuilderPass& pass = m_passes[i];

        RGResourceUsage usage{};
        usage.resource = ref;
        usage.pass_idx = i;
        usage.resource_type = RGResourceType::Buffer;

        for (const ShaderParameterMemberInfo& info : pass.get_buffer_view_members())
        {
            RGBufferViewRef view_ref = RGBufferViewRef::invalid();

            switch (info.mem_type)
            {
            case ShaderParameterMemberType::RGBufferSrv: {
                view_ref = std::get<RGBufferSrvRef>(info.value);
                usage.usage = RGResourceUsageType::Read;
                break;
            }
            case ShaderParameterMemberType::RGBufferUav: {
                view_ref = std::get<RGBufferUavRef>(info.value);
                usage.usage = RGResourceUsageType::ReadWrite;
                break;
            }
            case ShaderParameterMemberType::RGBufferCbv: {
                view_ref = std::get<RGBufferCbvRef>(info.value);
                usage.usage = RGResourceUsageType::ConstantBuffer;
                break;
            default:
                MIZU_UNREACHABLE("Invalid buffer view type");
            }
            }

            if (buffer_view_references_buffer(view_ref, ref))
            {
                usages.push_back(usage);
            }
        }
    }
}

FramebufferAttachment RenderGraphBuilder::create_framebuffer_attachment(
    RenderGraph& rg,
    const RGTextureRtvRef& rtv_ref,
    const RGImageMap& image_resources,
    const RGTextureViewsWrapper& texture_views,
    const RGResourceUsageMap& resource_usages,
    size_t pass_idx)
{
    MIZU_PROFILE_SCOPED;

    const RGTextureViewDescription& view_desc = get_texture_view_description(rtv_ref);

    const std::shared_ptr<ImageResource>& image = image_resources.find(view_desc.image_ref)->second;
    const std::shared_ptr<RenderTargetView>& rtv = texture_views.get(rtv_ref);

    MIZU_ASSERT(
        resource_usages.contains(view_desc.image_ref),
        "Image with id {} is not registered in image_usages",
        static_cast<UUID::Type>(view_desc.image_ref));
    const std::vector<RGResourceUsage>& usages = resource_usages.find(view_desc.image_ref)->second;

    const auto& it_usages = std::find_if(
        usages.begin(), usages.end(), [pass_idx](RGResourceUsage usage) { return usage.pass_idx == pass_idx; });
    MIZU_ASSERT(it_usages != usages.end(), "If texture is attachment of a framebuffer, should have at least one usage");

    const size_t usage_pos = static_cast<size_t>(it_usages - usages.begin());

    const bool is_image_external =
        std::find_if(
            m_external_image_descriptions.begin(),
            m_external_image_descriptions.end(),
            [ref = view_desc.image_ref](const std::pair<RGImageRef, RGExternalImageDescription>& pair) {
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
        const RGResourceUsage& previous_usage = usages[usage_pos - 1];

        const ImageResourceViewRange range = view_desc.range;
        // TODO: TEMPORAL, FIX
        // const ImageResourceViewRange previous_range = get_texture_view_description(previous_usage.view).range;
        const ImageResourceViewRange previous_range = range;
        //

        if (range != previous_range)
        {
            // If the previous usage ranges (mip or layer) are different, we need to treat the attachment as it has
            // never been used so that it get's cleared.
            initial_state = ImageResourceState::Undefined;
            load_operation = LoadOperation::Clear;
        }
        else
        {
            switch (previous_usage.usage)
            {
            case RGResourceUsageType::Attachment:
                initial_state = is_depth_format(image->get_format()) ? ImageResourceState::DepthStencilAttachment
                                                                     : ImageResourceState::ColorAttachment;

                load_operation = LoadOperation::Load;
                break;
            case RGResourceUsageType::Read:
                initial_state = ImageResourceState::ShaderReadOnly;
                // TODO: Revisit this comment and load operation
                // Loading just in case is a texture that is, for example, a depth buffer that is first read
                // from and then written to again
                load_operation = LoadOperation::Load;
                break;
            case RGResourceUsageType::ReadWrite:
                initial_state = ImageResourceState::UnorderedAccess;
                load_operation = LoadOperation::Load;
                break;
            case RGResourceUsageType::ConstantBuffer:
                // Does not apply
                break;
            }
        }
    }

    ImageResourceState final_state = is_depth_format(image->get_format()) ? ImageResourceState::DepthStencilAttachment
                                                                          : ImageResourceState::ColorAttachment;
    StoreOperation store_operation = StoreOperation::Store;

    const bool is_last_usage = usage_pos == usages.size() - 1;
    if (is_last_usage && !is_image_external)
    {
        // If is last usage and is not external, we don't care about store operation
        store_operation = StoreOperation::DontCare;
    }
    else if (is_last_usage && is_image_external)
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
    if (is_depth_format(image->get_format()))
    {
        clear_value = glm::vec4(1.0f);
    }

    FramebufferAttachment attachment{};
    attachment.rtv = rtv;
    attachment.load_operation = load_operation;
    attachment.store_operation = store_operation;
    attachment.initial_state = is_depth_format(image->get_format()) ? ImageResourceState::DepthStencilAttachment
                                                                    : ImageResourceState::ColorAttachment;
    attachment.final_state = attachment.initial_state;
    attachment.clear_value = clear_value;

    // Prepare image for attachment usage
    add_image_transition_pass(rg, *image, initial_state, final_state, view_desc.range);

    return attachment;
}

ImageFormat RenderGraphBuilder::get_image_format(RGImageRef ref) const
{
    const auto transient_it = m_transient_image_descriptions.find(ref);
    if (transient_it != m_transient_image_descriptions.end())
    {
        return transient_it->second.image_desc.format;
    }

    const auto external_it = m_external_image_descriptions.find(ref);
    if (external_it != m_external_image_descriptions.end())
    {
        return external_it->second.resource->get_format();
    }

    MIZU_UNREACHABLE("Image with id {} does not exist in transient or external images", static_cast<UUID::Type>(ref));
    return ImageFormat::B8G8R8A8_SRGB; // Default return value to prevent compilation errors
}

const RenderGraphBuilder::RGTextureViewDescription& RenderGraphBuilder::get_texture_view_description(
    RGTextureViewRef ref) const
{
    const auto it = m_transient_texture_view_descriptions.find(ref);
    MIZU_ASSERT(
        it != m_transient_texture_view_descriptions.end(),
        "Transient texture view with id {} does not exist",
        static_cast<UUID::Type>(ref));

    return it->second;
}

bool RenderGraphBuilder::texture_view_references_image(RGTextureViewRef view_ref, RGImageRef image_ref) const
{
    return get_image_from_texture_view(view_ref) == image_ref;
}

RGImageRef RenderGraphBuilder::get_image_from_texture_view(RGTextureViewRef view_ref) const
{
    const auto view_it = m_transient_texture_view_descriptions.find(view_ref);
    MIZU_ASSERT(
        view_it != m_transient_texture_view_descriptions.end(),
        "Transient image view with id {} does not exist",
        static_cast<UUID::Type>(view_ref));

    return view_it->second.image_ref;
}

bool RenderGraphBuilder::buffer_view_references_buffer(RGBufferViewRef view_ref, RGBufferRef buffer_ref) const
{
    return get_buffer_from_buffer_view(view_ref) == buffer_ref;
}

RGBufferRef RenderGraphBuilder::get_buffer_from_buffer_view(RGBufferViewRef view_ref) const
{
    const auto view_it = m_transient_buffer_view_descriptions.find(view_ref);
    MIZU_ASSERT(
        view_it != m_transient_buffer_view_descriptions.end(),
        "Transient buffer view with id {} does not exist",
        static_cast<UUID::Type>(view_ref));

    return view_it->second.buffer_ref;
}

//
// RenderGraph Passes
//

void RenderGraphBuilder::add_image_transition_pass(
    RenderGraph& rg,
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state) const
{
    add_image_transition_pass(rg, image, old_state, new_state, ImageResourceViewRange::from_mips_layers(0, 1, 0, 1));
}

void RenderGraphBuilder::add_image_transition_pass(
    RenderGraph& rg,
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state,
    ImageResourceViewRange range) const
{
    MIZU_PROFILE_SCOPED;

    if (old_state != new_state)
    {
        rg.m_passes.push_back([&image, old_ = old_state, new_ = new_state, range](CommandBuffer& _command) {
            _command.transition_resource(image, old_, new_, range);
        });
    }
}

void RenderGraphBuilder::add_buffer_transition_pass(
    RenderGraph& rg,
    const BufferResource& buffer,
    BufferResourceState old_state,
    BufferResourceState new_state)
{
    MIZU_PROFILE_SCOPED;

    if (old_state != new_state)
    {
        rg.m_passes.push_back([&buffer, old_ = old_state, new_ = new_state](CommandBuffer& _command) {
            _command.transition_resource(buffer, old_, new_);
        });
    }
}

void RenderGraphBuilder::add_copy_to_image_pass(
    RenderGraph& rg,
    const BufferResource& staging,
    const ImageResource& image) const
{
    MIZU_PROFILE_SCOPED;

    rg.m_passes.push_back([&staging, &image](CommandBuffer& _command) {
        _command.transition_resource(image, ImageResourceState::Undefined, ImageResourceState::TransferDst);
        _command.copy_buffer_to_image(staging, image);
    });
}

void RenderGraphBuilder::add_copy_to_buffer_pass(
    RenderGraph& rg,
    const BufferResource& staging,
    const BufferResource& buffer) const
{
    MIZU_PROFILE_SCOPED;

    rg.m_passes.push_back(
        [&staging, &buffer](CommandBuffer& _command) { _command.copy_buffer_to_buffer(staging, buffer); });
}

void RenderGraphBuilder::add_pass(
    RenderGraph& rg,
    const std::string& name,
    const RGPassResources& resources,
    const RGFunction& func) const
{
    MIZU_PROFILE_SCOPED;

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

} // namespace Mizu
