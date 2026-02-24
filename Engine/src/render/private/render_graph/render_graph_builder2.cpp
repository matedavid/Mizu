#include "render/render_graph/render_graph_builder2.h"

#include <algorithm>
#include <array>
#include <queue>

#include "base/debug/profiling.h"
#include "render_core/rhi/command_buffer.h"

#include "render/render_graph/render_graph2.h"
#include "render/runtime/renderer.h"
#include "render_graph/render_graph_resource_aliasing2.h"

namespace Mizu
{

/*
inline static bool render_graph_is_input_resource_usage(RenderGraphResourceUsageBits usage)
{
    return usage == RenderGraphResourceUsageBits::Read || usage == RenderGraphResourceUsageBits::CopySrc;
}
*/

inline static bool render_graph_is_output_resource_usage(RenderGraphResourceUsageBits usage)
{
    return usage == RenderGraphResourceUsageBits::Write || usage == RenderGraphResourceUsageBits::Attachment
           || usage == RenderGraphResourceUsageBits::CopyDst;
}

//
// RenderGraphPassResources2
//

ResourceView RenderGraphPassResources2::get_resource_view(RenderGraphResource resource) const
{
    (void)resource;
    return ResourceView{};
}

std::shared_ptr<BufferResource> RenderGraphPassResources2::get_buffer(RenderGraphResource resource) const
{
    const auto it = m_buffer_map.find(resource);
    MIZU_ASSERT(it != m_buffer_map.end(), "No buffer with id {} found on RenderGraphPassResources2", resource.id);

    return it->second.resource;
}

std::shared_ptr<ImageResource> RenderGraphPassResources2::get_image(RenderGraphResource resource) const
{
    const auto it = m_image_map.find(resource);
    MIZU_ASSERT(it != m_image_map.end(), "No image with id {} found on RenderGraphPassResources2", resource.id);

    return it->second.resource;
}

void RenderGraphPassResources2::add_resource(
    RenderGraphResource resource,
    std::shared_ptr<BufferResource> buffer,
    RenderGraphResourceUsageBits usage)
{
    BufferResourceUsage buffer_usage{};
    buffer_usage.resource = buffer;
    buffer_usage.usage = usage;

    m_buffer_map.insert({resource, buffer_usage});
}

void RenderGraphPassResources2::add_resource(
    RenderGraphResource resource,
    std::shared_ptr<ImageResource> image,
    RenderGraphResourceUsageBits usage)
{
    ImageResourceUsage image_usage{};
    image_usage.resource = image;
    image_usage.usage = usage;

    m_image_map.insert({resource, image_usage});
}

void RenderGraphPassResources2::add_resource(
    RenderGraphResource resource,
    std::shared_ptr<AccelerationStructure> acceleration_structure,
    RenderGraphResourceUsageBits usage)
{
    (void)resource;
    (void)acceleration_structure;
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

    RenderGraphAccessRecord* prev_access = record.prev;
    while (prev_access != nullptr)
    {
        if (render_graph_is_output_resource_usage(prev_access->usage))
        {
            RenderGraphPassBuilder2& prev_builder = m_builder.m_passes[prev_access->pass_idx];

            bool already_present = false;
            for (size_t value : prev_builder.m_pass_outputs)
            {
                if (value == m_pass_idx)
                {
                    already_present = true;
                    break;
                }
            }

            if (!already_present)
            {
                prev_builder.m_pass_outputs.push_back(m_pass_idx);
                m_pass_inputs.push_back(prev_access->pass_idx);
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
    resource_desc.concurrent_usage = false;
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
    resource_desc.concurrent_usage = false;
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

void RenderGraphBuilder2::compile(RenderGraph2& graph)
{
    MIZU_PROFILE_SCOPED;

    graph.reset();

    MIZU_ASSERT(!m_passes.empty(), "Can't compile RenderGraph without passes");

    const DeviceProperties& device_props = g_render_device->get_properties();
    const bool async_compute_enabled = m_config.async_compute_enabled && device_props.async_compute;

    if (m_config.async_compute_enabled != async_compute_enabled)
    {
        MIZU_LOG_WARNING("Can't enable async compute because the device does not support it");
    }

    //
    // Graph analysis
    //

    const auto topology_sort_cmp = [&](size_t a, size_t b) -> bool {
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

    const auto pass_hint_to_command_buffer_type = [&](RenderGraphPassHint hint) {
        switch (hint)
        {
        case RenderGraphPassHint::Raster:
        case RenderGraphPassHint::Compute:
        case RenderGraphPassHint::RayTracing:
        case RenderGraphPassHint::Transfer:
            return CommandBufferType::Graphics;
        case RenderGraphPassHint::AsyncCompute:
            return async_compute_enabled ? CommandBufferType::Compute : CommandBufferType::Graphics;
        }
    };

    constexpr size_t BITSET_SIZE = sizeof(uint64_t);
    const size_t words = (m_passes.size() + BITSET_SIZE - 1) / BITSET_SIZE;

    const auto set_bit = [&](std::vector<uint64_t>& bits, size_t i) {
        bits[i / BITSET_SIZE] |= (1ULL << (i % BITSET_SIZE));
    };

    const auto test_bit = [&](const std::vector<uint64_t>& bits, size_t i) -> bool {
        return (bits[i / BITSET_SIZE] >> (i % BITSET_SIZE)) & 1;
    };

    const auto union_bits = [&](std::vector<uint64_t>& dst, const std::vector<uint64_t>& src) {
        for (size_t w = 0; w < words; ++w)
            dst[w] |= src[w];
    };

    const auto add_unique = [](inplace_vector<size_t, RENDER_GRAPH_MAX_PASS_DEPENDENCIES>& vec, size_t value) {
        for (size_t v : vec)
        {
            if (v == value)
                return;
        }
        vec.push_back(value);
    };

    // Topologically sort graph

    std::vector<uint32_t> in_degree(m_passes.size(), 0);
    std::vector<bool> has_cross_queue_dep(m_passes.size(), false);
    std::vector<bool> has_cross_queue_out(m_passes.size(), false);

    for (const RenderGraphPassBuilder2& pass_info : m_passes)
    {
        const CommandBufferType pass_type = pass_hint_to_command_buffer_type(pass_info.m_hint);

        for (size_t output_idx : pass_info.m_pass_outputs)
        {
            in_degree[output_idx] += 1;

            // Check if the output has a cross queue dependencies
            const CommandBufferType output_type = pass_hint_to_command_buffer_type(m_passes[output_idx].m_hint);

            if (output_type != pass_type)
            {
                has_cross_queue_dep[output_idx] = true;
                has_cross_queue_out[pass_info.m_pass_idx] = true;
            }
        }
    }

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

    // Transitive reduction

    std::vector<std::vector<uint64_t>> reachable_vec(sorted_topology.size(), std::vector<uint64_t>(words, 0));

    for (int64_t pass_idx = sorted_topology.size() - 1; pass_idx >= 0; --pass_idx)
    {
        const RenderGraphPassBuilder2& pass_info = m_passes[pass_idx];
        for (size_t output_idx : pass_info.m_pass_outputs)
        {
            set_bit(reachable_vec[pass_idx], output_idx);
            union_bits(reachable_vec[pass_idx], reachable_vec[output_idx]);
        }
    }

    for (size_t pass_idx : sorted_topology)
    {
        RenderGraphPassBuilder2& pass_info = m_passes[pass_idx];

        pass_info.m_pass_outputs.erase(
            std::remove_if(
                pass_info.m_pass_outputs.begin(),
                pass_info.m_pass_outputs.end(),
                [&](size_t output_idx) -> bool {
                    for (size_t adj_idx : pass_info.m_pass_outputs)
                    {
                        if (output_idx != adj_idx && test_bit(reachable_vec[adj_idx], output_idx))
                        {
                            // Also remove the corresponding input from the output pass
                            auto& inputs = m_passes[output_idx].m_pass_inputs;
                            inputs.erase(std::remove(inputs.begin(), inputs.end(), pass_idx), inputs.end());

                            return true;
                        }
                    }
                    return false;
                }),
            pass_info.m_pass_outputs.end());
    }

    // Merge passes

    std::vector<CommandBufferBatch>& batches = graph.m_command_buffer_batches;
    batches.reserve(m_passes.size());

    std::vector<size_t> pass_to_batch(m_passes.size(), std::numeric_limits<size_t>::max());

    constexpr size_t INVALID_BATCH_IDX = std::numeric_limits<size_t>::max();
    std::array<size_t, enum_metadata_count_v<CommandBufferType>> last_unsealed = {
        INVALID_BATCH_IDX, INVALID_BATCH_IDX, INVALID_BATCH_IDX};

    for (size_t pass_idx : sorted_topology)
    {
        const RenderGraphPassBuilder2& pass_info = m_passes[pass_idx];
        const CommandBufferType pass_type = pass_hint_to_command_buffer_type(pass_info.m_hint);
        const size_t pass_type_idx = static_cast<size_t>(pass_type);

        size_t target_batch_idx = last_unsealed[pass_type_idx];

        if (target_batch_idx != INVALID_BATCH_IDX && !has_cross_queue_dep[pass_idx])
        {
            batches[target_batch_idx].pass_indices.push_back(pass_idx);
        }
        else
        {
            if (target_batch_idx != INVALID_BATCH_IDX)
            {
                batches[target_batch_idx].sealed = true;
            }

            const size_t new_idx = batches.size();

            CommandBufferBatch& new_batch = batches.emplace_back();
            new_batch.idx = new_idx;
            new_batch.type = pass_type;
            new_batch.pass_indices.push_back(pass_idx);

            last_unsealed[pass_type_idx] = new_idx;
            target_batch_idx = new_idx;
        }

        pass_to_batch[pass_idx] = target_batch_idx;

        // Build batch dependencies using precomputed inputs
        for (size_t input_pass_idx : pass_info.m_pass_inputs)
        {
            const size_t input_batch_idx = pass_to_batch[input_pass_idx];
            if (input_batch_idx != target_batch_idx)
            {
                add_unique(batches[target_batch_idx].incoming_batch_indices, input_batch_idx);
                add_unique(batches[input_batch_idx].outgoing_batch_indices, target_batch_idx);
            }
        }

        if (has_cross_queue_out[pass_idx])
        {
            batches[target_batch_idx].sealed = true;
            last_unsealed[pass_type_idx] = INVALID_BATCH_IDX;
        }
    }

    // Resource sharing info

    const size_t batch_words = (batches.size() + BITSET_SIZE - 1) / BITSET_SIZE;
    std::vector<std::vector<uint64_t>> batch_reachable(batches.size(), std::vector<uint64_t>(batch_words, 0));

    for (int64_t batch_idx = batches.size() - 1; batch_idx >= 0; --batch_idx)
    {
        for (size_t out_batch : batches[batch_idx].outgoing_batch_indices)
        {
            set_bit(batch_reachable[batch_idx], out_batch);
            union_bits(batch_reachable[batch_idx], batch_reachable[out_batch]);
        }
    }

    std::vector<std::vector<size_t>> resource_batches(m_resources.size());

    for (size_t pass_idx = 0; pass_idx < m_passes.size(); ++pass_idx)
    {
        const RenderGraphPassBuilder2& pass_info = m_passes[pass_idx];
        const size_t batch_idx = pass_to_batch[pass_idx];

        for (const RenderGraphAccessRecord& access : pass_info.m_accesses)
        {
            std::vector<size_t>& batches_vec = resource_batches[access.resource.id];

            bool already_present = false;
            for (size_t b : batches_vec)
            {
                if (b == batch_idx)
                {
                    already_present = true;
                    break;
                }
            }

            if (!already_present)
            {
                batches_vec.push_back(batch_idx);
            }
        }
    }

    for (size_t res_idx = 0; res_idx < m_resources.size(); ++res_idx)
    {
        RenderGraphResourceDescription& desc = m_resources[res_idx];
        const std::vector<size_t>& using_batches = resource_batches[res_idx];

        for (size_t batch_idx : using_batches)
        {
            desc.used_queue_types.set(batches[batch_idx].type);
        }

        if (desc.used_queue_types.count() <= 1)
            continue;

        for (size_t i = 0; i < using_batches.size() && !desc.concurrent_usage; ++i)
        {
            for (size_t j = i + 1; j < using_batches.size() && !desc.concurrent_usage; ++j)
            {
                const size_t b1 = using_batches[i];
                const size_t b2 = using_batches[j];

                if (batches[b1].type != batches[b2].type)
                {
                    // if !reachable(b1 -> b2) and !reachable(b2 - >b1) it means that there is not dependency/path
                    // between the two (b1 -> b2, b2 -> b1), therefore they can run concurrently.
                    if (!test_bit(batch_reachable[b1], b2) && !test_bit(batch_reachable[b2], b1))
                    {
                        desc.concurrent_usage = true;
                    }
                }
            }
        }
    }

    //
    // Create resources
    //

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

            if (resource_desc.concurrent_usage)
            {
                desc.sharing_mode = ResourceSharingMode::Concurrent;
                desc.queue_families = resource_desc.used_queue_types;
            }

            const auto buffer = g_render_device->create_buffer(desc);
            buffer_resources_map.insert({resource_desc.resource, buffer});

            memory_reqs = buffer->get_memory_requirements();

            break;
        }
        case RenderGraphResourceType::Texture: {
            ImageDescription desc = resource_desc.image();
            desc.usage |= get_image_usage_bits(resource_desc.usage);
            desc.is_virtual = true;

            if (resource_desc.concurrent_usage)
            {
                desc.sharing_mode = ResourceSharingMode::Concurrent;
                desc.queue_families = resource_desc.used_queue_types;
            }

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

    //
    // Create passes
    //

    for (CommandBufferBatch& batch : batches)
    {
        for (size_t pass_idx : batch.pass_indices)
        {
            const RenderGraphPassBuilder2& pass_info = m_passes[pass_idx];

            if (!validate_render_pass_builder(pass_info))
            {
                continue;
            }

            RenderGraphPassResources2& pass_resources = graph.m_pass_resources.emplace_back();

            // Add resource transitions
            for (const RenderGraphAccessRecord& access : pass_info.get_access_records())
            {
                const RenderGraphResourceDescription& resource_desc = get_resource_desc(access.resource);

                switch (resource_desc.type)
                {
                case RenderGraphResourceType::Buffer: {
                    const auto& buffer = buffer_resources_map[resource_desc.resource];
                    pass_resources.add_resource(resource_desc.resource, buffer, access.usage);

                    const BufferResource& buffer_resource = *pass_resources.get_buffer(resource_desc.resource);
                    add_buffer_acquire_transition(batch, buffer_resource, access, batches, pass_to_batch);

                    break;
                }
                case RenderGraphResourceType::Texture: {
                    const auto& image = image_resources_map[resource_desc.resource];
                    pass_resources.add_resource(resource_desc.resource, image, access.usage);

                    const ImageResource& image_resource = *pass_resources.get_image(resource_desc.resource);
                    add_image_acquire_transition(batch, image_resource, access, batches, pass_to_batch);

                    break;
                }
                case RenderGraphResourceType::AccelerationStructure: {
                    MIZU_UNREACHABLE("Not implemented");
                    break;
                }
                }
            }

            // Add pass execution function
            const PassExecuteCmd cmd{std::move(pass_info.m_execute_func), pass_resources};
            batch.commands.push_back(cmd);

            // Check if any release barriers or external resource transitions are needed and add them
            for (const RenderGraphAccessRecord& access : pass_info.get_access_records())
            {
                const RenderGraphResourceDescription& resource_desc = get_resource_desc(access.resource);

                switch (resource_desc.type)
                {
                case RenderGraphResourceType::Buffer: {
                    const BufferResource& buffer_resource = *pass_resources.get_buffer(resource_desc.resource);
                    add_buffer_release_transition(batch, buffer_resource, access, batches, pass_to_batch);
                    break;
                }
                case RenderGraphResourceType::Texture: {
                    const ImageResource& image_resource = *pass_resources.get_image(resource_desc.resource);
                    add_image_release_transition(batch, image_resource, access, batches, pass_to_batch);
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
}

static BufferResourceState render_graph_usage_to_buffer_resource_state(RenderGraphResourceUsageBits usage)
{
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

void RenderGraphBuilder2::add_buffer_acquire_transition(
    CommandBufferBatch& batch,
    const BufferResource& buffer,
    const RenderGraphAccessRecord& access,
    std::span<const CommandBufferBatch> batches,
    std::span<const size_t> pass_to_batch)
{
    BufferResourceState initial_state = BufferResourceState::Undefined, final_state = BufferResourceState::Undefined;
    std::optional<CommandBufferType> src_queue_type, dst_queue_type;
    ResourceTransitionMode transition_mode = ResourceTransitionMode::Normal;

    const RenderGraphResourceDescription& resource_desc = get_resource_desc(access.resource);
    const bool is_first_usage = access.prev == nullptr;

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

        if (!resource_desc.concurrent_usage)
        {
            // If prev access is on another batch with different queue, and resource is exclusive sharing mode,
            // add acquire barrier

            const size_t prev_batch_idx = pass_to_batch[prev_access.pass_idx];
            const CommandBufferBatch& prev_batch = batches[prev_batch_idx];

            if (prev_batch_idx != batch.idx && prev_batch.type != batch.type)
            {
                src_queue_type = prev_batch.type;
                dst_queue_type = batch.type;
                transition_mode = ResourceTransitionMode::Acquire;
            }
        }

        MIZU_ASSERT(initial_state != BufferResourceState::Undefined, "Invalid initial state");
    }

    {
        final_state = render_graph_usage_to_buffer_resource_state(access.usage);

        MIZU_ASSERT(final_state != BufferResourceState::Undefined, "Invalid final state");
    }

    if (initial_state == final_state)
        return;

    const BufferTransitionCmd transition_cmd{
        buffer, initial_state, final_state, src_queue_type, dst_queue_type, transition_mode};
    batch.commands.push_back(transition_cmd);

    MIZU_LOG_INFO(
        "Buffer transition: {} -> {}",
        buffer_resource_state_to_string(initial_state),
        buffer_resource_state_to_string(final_state));
}

void RenderGraphBuilder2::add_buffer_release_transition(
    CommandBufferBatch& batch,
    const BufferResource& buffer,
    const RenderGraphAccessRecord& access,
    std::span<const CommandBufferBatch> batches,
    std::span<const size_t> pass_to_batch)
{
    BufferResourceState initial_state = BufferResourceState::Undefined, final_state = BufferResourceState::Undefined;
    std::optional<CommandBufferType> src_queue_type, dst_queue_type;
    ResourceTransitionMode transition_mode = ResourceTransitionMode::Normal;

    const RenderGraphResourceDescription& resource_desc = get_resource_desc(access.resource);
    const bool is_last_usage = access.next == nullptr;

    {
        initial_state = render_graph_usage_to_buffer_resource_state(access.usage);
        MIZU_ASSERT(initial_state != BufferResourceState::Undefined, "Invalid initial state");
    }

    if (is_last_usage && resource_desc.is_external())
    {
        const RenderGraphExternalResourceDescription& external_desc =
            m_external_resources[resource_desc.external_index];
        final_state = external_desc.buffer_state().final_state;
    }
    else if (is_last_usage)
    {
        // If is last usage and not external, don't add transition
        return;
    }
    else
    {
        MIZU_ASSERT(access.next != nullptr, "If it's not last usage, it must have next usage");

        const RenderGraphAccessRecord& next_access = *access.next;
        final_state = render_graph_usage_to_buffer_resource_state(next_access.usage);

        if (!resource_desc.concurrent_usage)
        {
            // If next access is on another batch with different queue, and resource is exclusive sharing mode,
            // add release barrier
            const size_t next_batch_idx = pass_to_batch[next_access.pass_idx];
            const CommandBufferBatch& next_batch = batches[next_batch_idx];
            if (next_batch_idx != batch.idx && next_batch.type != batch.type)
            {
                src_queue_type = batch.type;
                dst_queue_type = next_batch.type;
                transition_mode = ResourceTransitionMode::Release;
            }
        }
    }

    if (initial_state == final_state)
        return;

    const BufferTransitionCmd transition_cmd{
        buffer, initial_state, final_state, src_queue_type, dst_queue_type, transition_mode};
    batch.commands.push_back(transition_cmd);

    MIZU_LOG_INFO(
        "Buffer transition: {} -> {}",
        buffer_resource_state_to_string(initial_state),
        buffer_resource_state_to_string(final_state));
}

static ImageResourceState render_graph_usage_to_image_resource_state(
    RenderGraphResourceUsageBits usage,
    ImageFormat format)
{
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
        if (is_depth_format(format))
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

void RenderGraphBuilder2::add_image_acquire_transition(
    CommandBufferBatch& batch,
    const ImageResource& image,
    const RenderGraphAccessRecord& access,
    std::span<const CommandBufferBatch> batches,
    std::span<const size_t> pass_to_batch)
{
    ImageResourceState initial_state = ImageResourceState::Undefined, final_state = ImageResourceState::Undefined;
    std::optional<CommandBufferType> src_queue_type, dst_queue_type;
    ResourceTransitionMode transfer_mode = ResourceTransitionMode::Normal;

    const RenderGraphResourceDescription& resource_desc = get_resource_desc(access.resource);
    const bool is_first_usage = access.prev == nullptr;

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
        initial_state = render_graph_usage_to_image_resource_state(prev_access.usage, image.get_format());

        if (!resource_desc.concurrent_usage)
        {
            const size_t prev_batch_idx = pass_to_batch[prev_access.pass_idx];
            const CommandBufferBatch& prev_batch = batches[prev_batch_idx];

            if (prev_batch_idx != batch.idx && prev_batch.type != batch.type)
            {
                src_queue_type = prev_batch.type;
                dst_queue_type = batch.type;
                transfer_mode = ResourceTransitionMode::Acquire;
            }
        }

        MIZU_ASSERT(initial_state != ImageResourceState::Undefined, "Invalid initial state");
    }

    {
        final_state = render_graph_usage_to_image_resource_state(access.usage, image.get_format());
        MIZU_ASSERT(final_state != ImageResourceState::Undefined, "Invalid final state");
    }

    if (initial_state == final_state)
        return;

    const ImageTransitionCmd transition_cmd{
        image, initial_state, final_state, src_queue_type, dst_queue_type, transfer_mode};
    batch.commands.push_back(transition_cmd);

    MIZU_LOG_INFO(
        "Image transition: {} -> {}",
        image_resource_state_to_string(initial_state),
        image_resource_state_to_string(final_state));
}

void RenderGraphBuilder2::add_image_release_transition(
    CommandBufferBatch& batch,
    const ImageResource& image,
    const RenderGraphAccessRecord& access,
    std::span<const CommandBufferBatch> batches,
    std::span<const size_t> pass_to_batch)
{
    ImageResourceState initial_state = ImageResourceState::Undefined, final_state = ImageResourceState::Undefined;
    std::optional<CommandBufferType> src_queue_type, dst_queue_type;
    ResourceTransitionMode transfer_mode = ResourceTransitionMode::Normal;

    const RenderGraphResourceDescription& resource_desc = get_resource_desc(access.resource);
    const bool is_last_usage = access.next == nullptr;

    {
        initial_state = render_graph_usage_to_image_resource_state(access.usage, image.get_format());
        MIZU_ASSERT(initial_state != ImageResourceState::Undefined, "Invalid initial state");
    }

    if (is_last_usage && resource_desc.is_external())
    {
        const RenderGraphExternalResourceDescription& external_desc =
            m_external_resources[resource_desc.external_index];
        final_state = external_desc.image_state().final_state;
    }
    else if (is_last_usage)
    {
        return;
    }
    else
    {
        MIZU_ASSERT(access.next != nullptr, "If it's not last usage, it must have next usage");

        const RenderGraphAccessRecord& next_access = *access.next;
        final_state = render_graph_usage_to_image_resource_state(next_access.usage, image.get_format());

        if (!resource_desc.concurrent_usage)
        {
            const size_t next_batch_idx = pass_to_batch[next_access.pass_idx];
            const CommandBufferBatch& next_batch = batches[next_batch_idx];
            if (next_batch_idx != batch.idx && next_batch.type != batch.type)
            {
                src_queue_type = batch.type;
                dst_queue_type = next_batch.type;
                transfer_mode = ResourceTransitionMode::Release;
            }
        }
    }

    if (initial_state == final_state)
        return;

    const ImageTransitionCmd transition_cmd{
        image, initial_state, final_state, src_queue_type, dst_queue_type, transfer_mode};
    batch.commands.push_back(transition_cmd);

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
