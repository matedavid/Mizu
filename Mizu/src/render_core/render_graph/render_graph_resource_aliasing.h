#pragma once

#include <iostream>
#include <numeric>
#include <queue>
#include <set>
#include <variant>
#include <vector>

#include "core/uuid.h"

namespace Mizu
{

#define MIZU_ALIAS_RESOURCES_DEBUG_ENABLED 0
#define MIZU_DISABLE_RESOURCE_ALIASING 0

struct RGResourceLifetime
{
    size_t begin, end;
    uint64_t size, offset;

    std::variant<RGImageRef, RGBufferRef> value;

    std::shared_ptr<TransientImageResource> transient_image;
    std::shared_ptr<TransientBufferResource> transient_buffer;
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

bool try_fit_in_node_r(Node* node, RGResourceLifetime* resource)
{
    if (resources_overlap(*node->resource, *resource))
    {
        return false;
    }

    if (node->children.empty())
    {
        resource->offset = 0;

        Node* child = new Node;
        child->size = resource->size;
        child->resource = resource;

        node->children.push_back(child);
        return true;
    }

    for (Node* child : node->children)
    {
        bool fits_in_children = try_fit_in_node_r(child, resource);
        if (fits_in_children)
        {
            return true;
        }
    }

    // Try fitting next to the other children
    auto& last = *(node->children.end() - 1);
    if (last->offset + last->size + resource->size < node->size)
    {
        Node* child = new Node;
        child->size = resource->size;
        child->offset = last->offset + last->size;
        child->resource = resource;
        child->resource->offset = last->offset + last->size;

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
    std::ranges::sort(resources,
                      [](const RGResourceLifetime& a, const RGResourceLifetime& b) { return a.size > b.size; });

    std::vector<RGResourceLifetime*> local_resources;

    for (auto& resource : resources)
    {
        local_resources.push_back(&resource);
    }

    std::vector<Node*> buckets;
    while (!local_resources.empty())
    {
        std::set<RGResourceLifetime*> aliased_resources;

        // First resource always creates bucket
        Node* parent = new Node;
        parent->size = local_resources[0]->size;
        parent->resource = local_resources[0];

        aliased_resources.insert(local_resources[0]);

        #if !MIZU_DISABLE_RESOURCE_ALIASING
        for (uint32_t i = 1; i < local_resources.size(); ++i)
        {
            bool did_fit = try_fit_in_node_r(parent, local_resources[i]);
            if (did_fit)
            {
                aliased_resources.insert(local_resources[i]);
            }
        }
        #endif

        buckets.push_back(parent);

        std::vector<RGResourceLifetime*> updated_resources;

        // auto it = local_resources.begin();
        for (auto resource : local_resources)
        {
            if (!aliased_resources.contains(resource))
            {
                updated_resources.push_back(resource);
            }
        }

        local_resources = updated_resources;
    }

    uint64_t offset = 0;
    for (auto node : buckets)
    {
        update_offset(node, offset);
        offset += node->size;
    }

    size_t total_size = 0;
    for (const Node* node : buckets)
    {
        total_size += node->size;
    }

#if MIZU_ALIAS_RESOURCES_DEBUG_ENABLED
    const std::function<void(const Node*, uint32_t)> print_node = [&](const Node* node, uint32_t offset) {
        if (node == nullptr)
        {
            return;
        }

        std::string offset_str = "";
        for (uint32_t i = 0; i < offset; ++i)
        {
            offset_str += "\t";
        }

        MIZU_LOG_INFO("{} Size: {} Offset: {} ({} - {})",
                      offset_str,
                      node->size,
                      node->offset,
                      node->resource->begin,
                      node->resource->end);

        for (const Node* child : node->children)
        {
            print_node(child, offset + 1);
        }
    };

    for (const Node* node : buckets)
    {
        print_node(node, 0);
    }
#endif

    for (Node* node : buckets)
    {
        delete_node(node);
    }

    return total_size;
}

} // namespace Mizu