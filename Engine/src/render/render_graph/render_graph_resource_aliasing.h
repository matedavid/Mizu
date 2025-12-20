#pragma once

#include <set>
#include <variant>
#include <vector>

#include "base/debug/profiling.h"
#include "base/types/uuid.h"

namespace Mizu
{

#define MIZU_RG_RESOURCE_ALIASING_DEBUG_ENABLED 0
#define MIZU_RG_DISABLE_RESOURCE_ALIASING 0

struct RGResourceLifetime
{
    size_t begin, end;
    uint64_t size, alignment, offset;

    std::variant<RGImageRef, RGBufferRef> value;

    std::shared_ptr<ImageResource> transient_image;
    std::shared_ptr<BufferResource> transient_buffer;
};

struct Node
{
    uint64_t size, offset;
    RGResourceLifetime* resource;

    std::vector<Node*> children;
};

bool resources_overlap(const RGResourceLifetime& r1, const RGResourceLifetime& r2)
{
    return r1.begin <= r2.end && r2.begin <= r1.end;
}

uint64_t compute_alignment_adjustment(uint64_t offset, uint64_t alignment)
{
    const uint64_t remainder = offset % alignment;
    return (remainder == 0) ? 0 : alignment - remainder;
}

bool try_fit_in_node_r(Node* node, RGResourceLifetime* resource, uint64_t size_to_current_bucket)
{
    if (resources_overlap(*node->resource, *resource))
    {
        return false;
    }

    // If node has no children, add directly as child
    if (node->children.empty())
    {
        const uint64_t alignment_size = compute_alignment_adjustment(size_to_current_bucket, resource->alignment);
        resource->offset = alignment_size;

        Node* child = new Node;
        child->size = resource->size;
        child->offset = alignment_size;
        child->resource = resource;

        node->children.push_back(child);
        return true;
    }

    // If node has children, try fit into one of those children
    for (Node* child : node->children)
    {
        bool fits_in_children = try_fit_in_node_r(child, resource, size_to_current_bucket);
        if (fits_in_children)
        {
            return true;
        }
    }

    // Try fitting next to the other children

    auto& last = *(node->children.end() - 1);

    const uint64_t alignment_size = compute_alignment_adjustment(last->offset + last->size, resource->alignment);

    const uint64_t fit_offset = last->offset + last->size + alignment_size;
    const uint64_t fit_size = fit_offset + resource->size;
    if (fit_size < node->size)
    {
        Node* child = new Node{};
        child->size = resource->size;
        child->offset = fit_offset;
        child->resource = resource;

        resource->offset = child->offset;

        node->children.push_back(child);
        return true;
    }

    return false;
}

void update_offset(Node* node, uint64_t to_add)
{
    node->offset += to_add;
    node->resource->offset += to_add;

    for (auto child : node->children)
    {
        update_offset(child, to_add);
    }
}

void delete_node(Node* node)
{
    for (Node* child : node->children)
    {
        delete_node(child);
    }

    delete node;
}

size_t alias_resources(std::vector<RGResourceLifetime>& resources)
{
    MIZU_PROFILE_SCOPED;

    std::ranges::sort(
        resources, [](const RGResourceLifetime& a, const RGResourceLifetime& b) { return a.size > b.size; });

    std::vector<RGResourceLifetime*> local_resources;

    for (auto& resource : resources)
    {
        local_resources.push_back(&resource);
    }

    uint64_t size_to_current_bucket = 0;

    std::vector<Node*> buckets;
    while (!local_resources.empty())
    {
        std::set<RGResourceLifetime*> aliased_resources;

        // First resource always creates bucket
        const uint64_t alignment_size =
            compute_alignment_adjustment(size_to_current_bucket, local_resources[0]->alignment);

        Node* parent = new Node;
        parent->size = local_resources[0]->size + alignment_size;
        parent->offset = alignment_size;
        parent->resource = local_resources[0];

        local_resources[0]->offset = parent->offset;

        aliased_resources.insert(local_resources[0]);

#if !MIZU_RG_DISABLE_RESOURCE_ALIASING
        for (uint32_t i = 1; i < local_resources.size(); ++i)
        {
            bool did_fit = try_fit_in_node_r(parent, local_resources[i], size_to_current_bucket);
            if (did_fit)
            {
                aliased_resources.insert(local_resources[i]);
            }
        }
#endif

        buckets.push_back(parent);

        std::vector<RGResourceLifetime*> updated_resources;

        for (auto resource : local_resources)
        {
            if (!aliased_resources.contains(resource))
            {
                updated_resources.push_back(resource);
            }
        }

        local_resources = updated_resources;

        size_to_current_bucket += parent->size;
    }

    uint64_t offset = 0;
    for (auto node : buckets)
    {
        update_offset(node, offset);
        offset += node->size;
    }

    uint64_t total_size = 0;
    for (const Node* node : buckets)
    {
        total_size += node->size;
    }

#if MIZU_DEBUG
    // Check alignment requirements
    for (const RGResourceLifetime& resource : resources)
    {
        MIZU_ASSERT(
            resource.offset % resource.alignment == 0,
            "Resource alignment requirement is not met (alignment = {}, offset = {})",
            resource.alignment,
            resource.offset);
    }
#endif

#if MIZU_RG_RESOURCE_ALIASING_DEBUG_ENABLED
    const std::function<void(const Node*, uint32_t)> print_node_r = [&](const Node* node, uint32_t separation) {
        if (node == nullptr)
        {
            return;
        }

        std::string offset_str = "";
        for (uint32_t i = 0; i < separation; ++i)
        {
            offset_str += " ";
        }

        std::string resource_name;
        if (node->resource->transient_buffer != nullptr)
        {
            resource_name = node->resource->transient_buffer->get_name();
        }
        else if (node->resource->transient_image != nullptr)
        {
            resource_name = node->resource->transient_image->get_name();
        }

        MIZU_LOG_INFO(
            "{}{} - Size: {} Offset: {} Alignment: {} ({} - {})",
            offset_str,
            resource_name,
            node->size,
            node->offset,
            node->resource->alignment,
            node->resource->begin,
            node->resource->end);

        for (const Node* child : node->children)
        {
            print_node_r(child, separation + 2);
        }
    };

    uint64_t non_aliased_memory_size = 0;
    for (const RGResourceLifetime& resource : resources)
    {
        non_aliased_memory_size +=
            resource.size + compute_alignment_adjustment(non_aliased_memory_size, resource.alignment);
    }

    const float size_ratio = static_cast<float>(total_size) / static_cast<float>(non_aliased_memory_size);

    MIZU_LOG_INFO("Aliasing info:");
    MIZU_LOG_INFO("  Number resources:        {}", resources.size());
    MIZU_LOG_INFO("  Non aliased memory size: {} bytes", non_aliased_memory_size);
    MIZU_LOG_INFO("  Aliased memory size:     {} bytes", total_size);
    MIZU_LOG_INFO("  Size ratio:              {}", size_ratio);
    MIZU_LOG_INFO("  Aliased resources:");

    for (const Node* node : buckets)
    {
        print_node_r(node, 6);
    }
#endif

    for (Node* node : buckets)
    {
        delete_node(node);
    }

    return total_size;
}

} // namespace Mizu