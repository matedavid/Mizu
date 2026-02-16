#include "render/render_graph/render_graph_builder2.h"

#include <queue>

#include "render_core/rhi/command_buffer.h"

#include "render/runtime/renderer.h"
#include "render_graph/render_graph_resource_aliasing2.h"

namespace Mizu
{

inline static bool render_graph_is_input_resource_usage(RenderGraphResourceUsageBits usage)
{
    return usage == RenderGraphResourceUsageBits::Read || usage == RenderGraphResourceUsageBits::CopySrc;
}

inline static bool render_graph_is_output_resource_usage(RenderGraphResourceUsageBits usage)
{
    return usage == RenderGraphResourceUsageBits::Write || usage == RenderGraphResourceUsageBits::Attachment
           || usage == RenderGraphResourceUsageBits::CopyDst;
}

//
// RenderGraphPassResources2
//

ResourceView RenderGraphPassResources2::get_resource(RenderGraphResource resource) const
{
    (void)resource;
    return ResourceView{};
}

std::shared_ptr<BufferResource> RenderGraphPassResources2::get_buffer(RenderGraphResource resource) const
{
    (void)resource;
    return nullptr;
}

std::shared_ptr<ImageResource> RenderGraphPassResources2::get_image(RenderGraphResource resource) const
{
    (void)resource;
    return nullptr;
}

void RenderGraphPassResources2::add_resource(
    RenderGraphResource resource,
    std::shared_ptr<BufferResource> buffer,
    RenderGraphResourceUsageBits usage)
{
    (void)resource;
    (void)buffer;
    (void)usage;
}

void RenderGraphPassResources2::add_resource(
    RenderGraphResource resource,
    std::shared_ptr<ImageResource> image,
    RenderGraphResourceUsageBits usage)
{
    (void)resource;
    (void)image;
    (void)usage;
}

//
// RenderGraphPassBuilder2
//

RenderGraphPassBuilder2::RenderGraphPassBuilder2(RenderGraphBuilder2& builder, std::string_view name, uint32_t pass_idx)
    : m_builder(builder)
    , m_hint(RenderGraphPassHint::Raster)
    , m_name(name)
    , m_pass_idx(pass_idx)
    , m_has_outputs(false)
{
}

void RenderGraphPassBuilder2::set_hint(RenderGraphPassHint hint)
{
    m_hint = hint;
}

RenderGraphResource RenderGraphPassBuilder2::read(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::Read);
}

RenderGraphResource RenderGraphPassBuilder2::write(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::Write);
}

RenderGraphResource RenderGraphPassBuilder2::attachment(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::Attachment);
}

RenderGraphResource RenderGraphPassBuilder2::copy_src(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::CopySrc);
}

RenderGraphResource RenderGraphPassBuilder2::copy_dst(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::CopyDst);
}

std::span<const RenderGraphAccessRecord> RenderGraphPassBuilder2::get_access_records() const
{
    return m_accesses;
}

RenderGraphResource RenderGraphPassBuilder2::add_resource_access(
    RenderGraphResource resource,
    RenderGraphResourceUsageBits usage)
{
    RenderGraphAccessRecord& record = m_accesses.emplace_back();
    record.resource = resource;
    record.usage = usage;
    record.pass_idx = m_pass_idx;

    RenderGraphResourceDescription& desc = m_builder.get_resource_desc(resource);
    desc.usage |= usage;

    populate_dependency_info(record, desc);

    desc.first_pass_idx = std::min(desc.first_pass_idx, m_pass_idx);
    desc.last_pass_idx = std::max(desc.last_pass_idx, m_pass_idx);

    m_has_outputs |= render_graph_is_output_resource_usage(usage);

    return resource;
}

void RenderGraphPassBuilder2::populate_dependency_info(
    RenderGraphAccessRecord& record,
    const RenderGraphResourceDescription& desc)
{
    if (desc.first_pass_idx == std::numeric_limits<uint32_t>::max())
    {
        record.prev = nullptr;
        return;
    }

    // TODO: Not the biggest fan of doing an iteration here, but at most it will be MAX_ACCESS_RECORDS_PER_PASS
    // iterations, so not the worst.
    RenderGraphPassBuilder2& pass_builder = m_builder.m_passes[desc.last_pass_idx];
    for (RenderGraphAccessRecord& prev_record : pass_builder.m_accesses)
    {
        if (prev_record.resource == desc.resource)
        {
            prev_record.next = &record;
            record.prev = &prev_record;

            break;
        }
    }
    MIZU_ASSERT(record.prev != nullptr, "Did not find a valid previous access record");

    /*
    if (render_graph_is_output_resource_usage(record.usage))
    {
        // If the current usage is an output usage, don't consider previous output usages as dependencies.
        return;
    }
    */

    /*
    RenderGraphAccessRecord* prev_access = record.prev;
    while (prev_access != nullptr)
    {
        if (render_graph_is_output_resource_usage(prev_access->usage))
        {
            for (size_t value : m_pass_dependencies)
            {
                if (value == prev_access->pass_idx)
                    break;
            }

            m_pass_dependencies.push_back(prev_access->pass_idx);

            break;
        }

        prev_access = prev_access->prev;
    }
    */

    RenderGraphAccessRecord* prev_access = record.prev;
    while (prev_access != nullptr)
    {
        if (render_graph_is_output_resource_usage(prev_access->usage))
        {
            RenderGraphPassBuilder2& prev_builder = m_builder.m_passes[prev_access->pass_idx];

            bool already_present = false;
            for (size_t value : prev_builder.m_pass_outputs)
            {
                if (value == prev_access->pass_idx)
                {
                    already_present = true;
                    break;
                }
            }

            if (!already_present)
            {
                prev_builder.m_pass_outputs.push_back(m_pass_idx);
            }

            break;
        }

        prev_access = prev_access->prev;
    }
}

//
// RenderGraphBuilder2
//

RenderGraphBuilder2::RenderGraphBuilder2(RenderGraphBuilder2Config config) : m_config(std::move(config))
{
    constexpr size_t PASSES_TO_RESERVE = 120;
    m_passes.reserve(PASSES_TO_RESERVE);
}

RenderGraphResource RenderGraphBuilder2::create_buffer(BufferDescription desc)
{
    RenderGraphResource resource_ref{};
    resource_ref.id = m_resources.size();

    RenderGraphResourceDescription& resource_desc = m_resources.emplace_back();
    resource_desc.resource = resource_ref;
    resource_desc.type = RenderGraphResourceType::Buffer;
    resource_desc.usage = RenderGraphResourceUsageBits::None;
    resource_desc.desc = std::move(desc);

    return resource_ref;
}

RenderGraphResource RenderGraphBuilder2::create_constant_buffer(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = 0u;
    desc.usage = BufferUsageBits::ConstantBuffer;
    desc.name = std::move(name);

    return create_buffer(desc);
}

RenderGraphResource RenderGraphBuilder2::create_structured_buffer(uint64_t size, uint64_t stride, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = stride;
    desc.usage = BufferUsageBits::None;
    desc.name = std::move(name);

    return create_buffer(desc);
}

RenderGraphResource RenderGraphBuilder2::create_texture(ImageDescription desc)
{
    RenderGraphResource resource_ref{};
    resource_ref.id = m_resources.size();

    RenderGraphResourceDescription& resource_desc = m_resources.emplace_back();
    resource_desc.resource = resource_ref;
    resource_desc.type = RenderGraphResourceType::Texture;
    resource_desc.usage = RenderGraphResourceUsageBits::None;
    resource_desc.desc = std::move(desc);

    return resource_ref;
}

RenderGraphResource RenderGraphBuilder2::create_texture2d(
    uint32_t width,
    uint32_t height,
    ImageFormat format,
    std::string name)
{
    ImageDescription desc{};
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.type = ImageType::Image2D;
    desc.format = format;
    desc.usage = ImageUsageBits::None;
    desc.name = std::move(name);

    return create_texture(desc);
}

RenderGraphResource RenderGraphBuilder2::register_external_buffer(
    std::shared_ptr<BufferResource> buffer,
    RenderGraphExternalBufferState state)
{
    RenderGraphResource resource_ref{};
    resource_ref.id = m_resources.size();

    RenderGraphResourceDescription& resource_desc = m_resources.emplace_back();
    resource_desc.resource = resource_ref;
    resource_desc.type = RenderGraphResourceType::Buffer;
    resource_desc.external_index = m_external_resources.size();

    RenderGraphExternalResourceDescription& external_resource_desc = m_external_resources.emplace_back();
    external_resource_desc.value = buffer;
    external_resource_desc.state = state;

    return resource_ref;
}

RenderGraphResource RenderGraphBuilder2::register_external_texture(
    std::shared_ptr<ImageResource> image,
    RenderGraphExternalImageState state)
{
    RenderGraphResource resource_ref{};
    resource_ref.id = m_resources.size();

    RenderGraphResourceDescription& resource_desc = m_resources.emplace_back();
    resource_desc.resource = resource_ref;
    resource_desc.type = RenderGraphResourceType::Texture;
    resource_desc.external_index = m_external_resources.size();

    RenderGraphExternalResourceDescription& external_resource_desc = m_external_resources.emplace_back();
    external_resource_desc.value = image;
    external_resource_desc.state = state;

    return resource_ref;
}

RenderGraphResource RenderGraphBuilder2::register_external_acceleration_structure(
    std::shared_ptr<AccelerationStructure> acceleration_structure)
{
    RenderGraphResource resource_ref{};
    resource_ref.id = m_resources.size();

    RenderGraphResourceDescription& resource_desc = m_resources.emplace_back();
    resource_desc.resource = resource_ref;
    resource_desc.type = RenderGraphResourceType::AccelerationStructure;
    resource_desc.external_index = m_external_resources.size();

    RenderGraphExternalResourceDescription& external_resource_desc = m_external_resources.emplace_back();
    external_resource_desc.value = acceleration_structure;

    return resource_ref;
}

void RenderGraphBuilder2::compile()
{
    MIZU_ASSERT(!m_passes.empty(), "Can't compile RenderGraph without passes");

    const DeviceProperties& device_props = g_render_device->get_properties();
    const bool async_compute_enabled = m_config.async_compute_enabled && device_props.async_compute;

    if (m_config.async_compute_enabled != async_compute_enabled)
    {
        MIZU_LOG_WARNING("Can't enable async compute because the device does not support it");
    }

    std::unordered_map<RenderGraphResource, std::shared_ptr<BufferResource>> buffer_resources_map;
    buffer_resources_map.reserve(m_resources.size());
    std::unordered_map<RenderGraphResource, std::shared_ptr<ImageResource>> image_resources_map;
    image_resources_map.reserve(m_resources.size());
    std::unordered_map<RenderGraphResource, std::shared_ptr<AccelerationStructure>>
        acceleration_structure_resources_map;
    acceleration_structure_resources_map.reserve(m_resources.size());

    std::vector<AliasingResource> aliasing_resources;
    aliasing_resources.reserve(m_resources.size());

    for (const RenderGraphResourceDescription& resource_desc : m_resources)
    {
        MIZU_LOG_INFO("Resource:");
        MIZU_LOG_INFO("  Type:       {}", render_graph_resource_type_to_string(resource_desc.type));
        MIZU_LOG_INFO("  Usage:      {}", static_cast<RenderGraphResourceUsageBitsType>(resource_desc.usage));
        MIZU_LOG_INFO("  Accesses:   ({},{})", resource_desc.first_pass_idx, resource_desc.last_pass_idx);
        MIZU_LOG_INFO("  IsExternal: {}", resource_desc.is_external());

        if (resource_desc.usage == RenderGraphResourceUsageBits::None)
        {
            MIZU_LOG_WARNING("Resource with id {} does not have any usages, ignoring", resource_desc.resource.id);
            continue;
        }

        if (resource_desc.is_external())
        {
            switch (resource_desc.type)
            {
            case RenderGraphResourceType::Buffer: {
                const RenderGraphExternalResourceDescription& external_desc =
                    get_external_resource_desc(resource_desc.resource);
                buffer_resources_map.insert({resource_desc.resource, external_desc.buffer()});
                break;
            }
            case RenderGraphResourceType::Texture: {
                const RenderGraphExternalResourceDescription& external_desc =
                    get_external_resource_desc(resource_desc.resource);
                image_resources_map.insert({resource_desc.resource, external_desc.image()});
                break;
            }
            case RenderGraphResourceType::AccelerationStructure: {
                const RenderGraphExternalResourceDescription& external_desc =
                    get_external_resource_desc(resource_desc.resource);
                acceleration_structure_resources_map.insert(
                    {resource_desc.resource, external_desc.acceleration_structure()});
                break;
            }
            }

            continue;
        }

        MemoryRequirements memory_reqs{};

        switch (resource_desc.type)
        {
        case RenderGraphResourceType::Buffer: {
            BufferDescription desc = resource_desc.buffer();
            desc.usage |= get_buffer_usage_bits(resource_desc.usage);
            desc.is_virtual = true;

            const auto buffer = g_render_device->create_buffer(desc);
            buffer_resources_map.insert({resource_desc.resource, buffer});

            memory_reqs = buffer->get_memory_requirements();

            break;
        }
        case RenderGraphResourceType::Texture: {
            ImageDescription desc = resource_desc.image();
            desc.usage |= get_image_usage_bits(resource_desc.usage);
            desc.is_virtual = true;

            const auto image = g_render_device->create_image(desc);
            image_resources_map.insert({resource_desc.resource, image});

            memory_reqs = image->get_memory_requirements();

            break;
        }
        case RenderGraphResourceType::AccelerationStructure: {
            MIZU_UNREACHABLE("Not implemented");
            break;
        }
        }

        AliasingResource aliasing_resource{};
        aliasing_resource.resource = resource_desc.resource;
        aliasing_resource.begin = resource_desc.first_pass_idx;
        aliasing_resource.end = resource_desc.last_pass_idx;
        aliasing_resource.size = memory_reqs.size;
        aliasing_resource.alignment = memory_reqs.alignment;

        aliasing_resources.push_back(aliasing_resource);
    }

    uint64_t total_size = 0;
    render_graph_alias_resources(aliasing_resources, total_size);

    // ============================

    std::vector<uint32_t> in_degree(m_passes.size(), 0);
    for (const RenderGraphPassBuilder2& pass_info : m_passes)
    {
        for (size_t output_idx : pass_info.m_pass_outputs)
        {
            in_degree[output_idx] += 1;
        }
    }

    const auto topology_sort_cmp = [&](size_t a, size_t b) {
        const RenderGraphPassBuilder2& pass_a = m_passes[a];
        const RenderGraphPassBuilder2& pass_b = m_passes[b];

        if (pass_a.m_hint != pass_b.m_hint && async_compute_enabled)
        {
            const bool a_is_async_compute = pass_a.m_hint == RenderGraphPassHint::AsyncCompute;
            const bool b_is_async_compute = pass_b.m_hint == RenderGraphPassHint::AsyncCompute;

            if (a_is_async_compute || b_is_async_compute)
            {
                // If b is async, b is the higher priority so return true  == 'a has lower priority'.
                // If a is async, a is the higher priority so return false == 'a has higher priority'.
                // If both are async, the order doesn't matter that much.
                return b_is_async_compute;
            }
        }

        return pass_a.m_pass_idx > pass_b.m_pass_idx;
    };

    std::priority_queue<size_t, std::vector<size_t>, decltype(topology_sort_cmp)> priority_queue(topology_sort_cmp);
    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        if (in_degree[i] == 0)
            priority_queue.push(i);
    }

    std::vector<size_t> sorted_topology;
    sorted_topology.reserve(m_passes.size());

    while (!priority_queue.empty())
    {
        const size_t top = priority_queue.top();
        priority_queue.pop();

        sorted_topology.push_back(top);

        for (size_t adj : m_passes[top].m_pass_outputs)
        {
            if (--in_degree[adj] == 0)
            {
                priority_queue.push(adj);
            }
        }
    }

    MIZU_ASSERT(sorted_topology.size() == m_passes.size(), "A cycle was detected in the RenderGraph");

    // Merge passes

    struct MergedPasses
    {
        CommandBufferType command_buffer_type;
        std::vector<size_t> passes;
    };

    const auto render_graph_pass_hint_to_command_buffer_type = [](RenderGraphPassHint hint) {
        switch (hint)
        {
        case RenderGraphPassHint::Raster:
        case RenderGraphPassHint::Compute:
        case RenderGraphPassHint::RayTracing:
        case RenderGraphPassHint::Transfer:
            return CommandBufferType::Graphics;
        case RenderGraphPassHint::AsyncCompute:
            return CommandBufferType::Compute;
        }
    };

    std::vector<MergedPasses> merged_passes;
    merged_passes.reserve(m_passes.size());

    size_t current_merged_pass_idx = 0;

    for (size_t pass_idx : sorted_topology)
    {
        const RenderGraphPassBuilder2& pass_info = m_passes[pass_idx];
        const CommandBufferType pass_command_buffer_type =
            render_graph_pass_hint_to_command_buffer_type(pass_info.m_hint);

        if (merged_passes.empty())
        {
            current_merged_pass_idx = 0;
            merged_passes.push_back(
                MergedPasses{
                    .command_buffer_type = pass_command_buffer_type,
                    .passes = {pass_idx},
                });

            continue;
        }

        MergedPasses& current_merged_pass = merged_passes[current_merged_pass_idx];

        if (current_merged_pass.command_buffer_type == pass_command_buffer_type)
        {
            current_merged_pass.passes.push_back(pass_idx);
        }
        else
        {
            merged_passes.push_back(
                MergedPasses{
                    .command_buffer_type = pass_command_buffer_type,
                    .passes = {pass_idx},
                });

            current_merged_pass_idx += 1;
        }
    }

    // ============================

    for (const RenderGraphPassBuilder2& pass_info : m_passes)
    {
        if (!validate_render_pass_builder(pass_info))
        {
            continue;
        }

        RenderGraphPassResources2 pass_resources{};

        for (const RenderGraphAccessRecord& access : pass_info.get_access_records())
        {
            const RenderGraphResourceDescription& resource_desc = get_resource_desc(access.resource);

            switch (resource_desc.type)
            {
            case RenderGraphResourceType::Buffer: {
                const auto& buffer = buffer_resources_map[resource_desc.resource];
                pass_resources.add_resource(resource_desc.resource, buffer, access.usage);

                enqueue_buffer_resource_state_transition(*buffer, access, resource_desc, pass_info);

                break;
            }
            case RenderGraphResourceType::Texture: {
                const auto& image = image_resources_map[resource_desc.resource];
                pass_resources.add_resource(resource_desc.resource, image, access.usage);

                enqueue_image_resource_state_transition(*image, access, resource_desc, pass_info);

                break;
            }
            case RenderGraphResourceType::AccelerationStructure: {
                MIZU_UNREACHABLE("Not implemented");
                break;
            }
            }
        }
    }
}

void RenderGraphBuilder2::enqueue_buffer_resource_state_transition(
    const BufferResource& buffer,
    const RenderGraphAccessRecord& access,
    const RenderGraphResourceDescription& resource_desc,
    const RenderGraphPassBuilder2& pass_info)
{
    const auto render_graph_usage_to_buffer_resource_state =
        [](RenderGraphResourceUsageBits usage) -> BufferResourceState {
        switch (usage)
        {
        case RenderGraphResourceUsageBits::None:
            MIZU_UNREACHABLE("Invalid usage bits");
            return BufferResourceState::Undefined;
        case RenderGraphResourceUsageBits::Read:
            return BufferResourceState::ShaderReadOnly;
        case RenderGraphResourceUsageBits::Write:
            return BufferResourceState::UnorderedAccess;
        case RenderGraphResourceUsageBits::Attachment:
            MIZU_UNREACHABLE("Invalid usage bits for buffer");
            return BufferResourceState::Undefined;
        case RenderGraphResourceUsageBits::CopySrc:
            return BufferResourceState::TransferSrc;
        case RenderGraphResourceUsageBits::CopyDst:
            return BufferResourceState::TransferDst;
        }
    };

    const bool is_first_usage = pass_info.m_pass_idx == resource_desc.first_pass_idx;
    const bool is_last_usage = pass_info.m_pass_idx == resource_desc.last_pass_idx;

    BufferResourceState initial_state = BufferResourceState::Undefined;
    BufferResourceState final_state = BufferResourceState::Undefined;

    if (is_first_usage && resource_desc.is_external())
    {
        const RenderGraphExternalResourceDescription& external_desc =
            m_external_resources[resource_desc.external_index];
        initial_state = external_desc.buffer_state().initial_state;
    }
    else if (is_first_usage)
    {
        initial_state = BufferResourceState::Undefined;
    }
    else
    {
        MIZU_ASSERT(access.prev != nullptr, "If it's not initial usage, it must have previous usage");

        const RenderGraphAccessRecord& prev_access = *access.prev;
        initial_state = render_graph_usage_to_buffer_resource_state(prev_access.usage);

        if (initial_state == BufferResourceState::Undefined)
            return;
    }

    if (is_last_usage && resource_desc.is_external())
    {
        const RenderGraphExternalResourceDescription& external_desc =
            m_external_resources[resource_desc.external_index];
        final_state = external_desc.buffer_state().final_state;
    }
    else
    {
        final_state = render_graph_usage_to_buffer_resource_state(access.usage);

        if (final_state == BufferResourceState::Undefined)
            return;
    }

    // TODO: actually prepare transition
    (void)buffer;

    MIZU_LOG_INFO(
        "Buffer transition: {} -> {}",
        buffer_resource_state_to_string(initial_state),
        buffer_resource_state_to_string(final_state));
}

void RenderGraphBuilder2::enqueue_image_resource_state_transition(
    const ImageResource& image,
    const RenderGraphAccessRecord& access,
    const RenderGraphResourceDescription& resource_desc,
    const RenderGraphPassBuilder2& pass_info)
{
    const auto render_graph_usage_to_image_resource_state =
        [&](RenderGraphResourceUsageBits usage) -> ImageResourceState {
        switch (usage)
        {
        case RenderGraphResourceUsageBits::None:
            MIZU_UNREACHABLE("Invalid usage bits");
            return ImageResourceState::Undefined;
        case RenderGraphResourceUsageBits::Read:
            return ImageResourceState::ShaderReadOnly;
        case RenderGraphResourceUsageBits::Write:
            return ImageResourceState::UnorderedAccess;
        case RenderGraphResourceUsageBits::Attachment: {
            if (is_depth_format(image.get_format()))
            {
                return ImageResourceState::DepthStencilAttachment;
            }
            else
            {
                return ImageResourceState::ColorAttachment;
            }
        }
        case RenderGraphResourceUsageBits::CopySrc:
            return ImageResourceState::TransferSrc;
        case RenderGraphResourceUsageBits::CopyDst:
            return ImageResourceState::TransferDst;
        }
    };

    const bool is_first_usage = pass_info.m_pass_idx == resource_desc.first_pass_idx;
    // const bool is_last_usage = pass_info.m_pass_idx == resource_desc.last_pass_idx;

    ImageResourceState initial_state = ImageResourceState::Undefined;
    ImageResourceState final_state = ImageResourceState::Undefined;

    if (is_first_usage && resource_desc.is_external())
    {
        const RenderGraphExternalResourceDescription& external_desc =
            m_external_resources[resource_desc.external_index];
        initial_state = external_desc.image_state().initial_state;
    }
    else if (is_first_usage)
    {
        initial_state = ImageResourceState::Undefined;
    }
    else
    {
        MIZU_ASSERT(access.prev != nullptr, "If it's not initial usage, it must have previous usage");

        const RenderGraphAccessRecord& prev_access = *access.prev;
        initial_state = render_graph_usage_to_image_resource_state(prev_access.usage);

        if (initial_state == ImageResourceState::Undefined)
            return;
    }

    /*
    if (is_last_usage && resource_desc.is_external())
    {
        const RenderGraphExternalResourceDescription& external_desc =
            m_external_resources[resource_desc.external_index];
        final_state = external_desc.image_state().final_state;
    }
    else
    */
    {
        final_state = render_graph_usage_to_image_resource_state(access.usage);

        if (final_state == ImageResourceState::Undefined)
            return;
    }

    // TODO: actually prepare transition
    (void)image;

    MIZU_LOG_INFO(
        "Image transition: {} -> {}",
        image_resource_state_to_string(initial_state),
        image_resource_state_to_string(final_state));
}

bool RenderGraphBuilder2::validate_render_pass_builder(const RenderGraphPassBuilder2& pass)
{
    if (!pass.m_has_outputs)
    {
        MIZU_LOG_WARNING("Pass '{}' has no outputs, culling", pass.m_name);
        return false;
    }

    return true;
}

BufferUsageBits RenderGraphBuilder2::get_buffer_usage_bits(RenderGraphResourceUsageBits usage)
{
    BufferUsageBits usage_bits = BufferUsageBits::None;

    if (usage & RenderGraphResourceUsageBits::Write)
        usage_bits |= BufferUsageBits::UnorderedAccess;

    if (usage & RenderGraphResourceUsageBits::CopySrc)
        usage_bits |= BufferUsageBits::TransferSrc;

    if (usage & RenderGraphResourceUsageBits::CopyDst)
        usage_bits |= BufferUsageBits::TransferDst;

    return usage_bits;
}

ImageUsageBits RenderGraphBuilder2::get_image_usage_bits(RenderGraphResourceUsageBits usage)
{
    ImageUsageBits usage_bits = ImageUsageBits::None;

    if (usage & RenderGraphResourceUsageBits::Read)
        usage_bits |= ImageUsageBits::Sampled;

    if (usage & RenderGraphResourceUsageBits::Write)
        usage_bits |= ImageUsageBits::UnorderedAccess;

    if (usage & RenderGraphResourceUsageBits::Attachment)
        usage_bits | ImageUsageBits::Attachment;

    if (usage & RenderGraphResourceUsageBits::CopySrc)
        usage_bits |= ImageUsageBits::TransferSrc;

    if (usage & RenderGraphResourceUsageBits::CopyDst)
        usage_bits |= ImageUsageBits::TransferDst;

    return usage_bits;
}

} // namespace Mizu