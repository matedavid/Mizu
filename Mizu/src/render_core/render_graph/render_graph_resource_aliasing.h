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

struct RGResourceLifetime
{
    size_t begin, end;
    size_t size, offset;

    std::variant<RGImageRef, RGBufferRef> value;

    std::shared_ptr<TransientImageResource> transient_image;
    std::shared_ptr<TransientBufferResource> transient_buffer;
};

struct Node
{
    uint32_t size, offset = 0;
    RGResourceLifetime* resource;

    std::vector<Node*> children;
};

bool resources_overlap(const RGResourceLifetime& r1, const RGResourceLifetime& r2)
{
    return r1.begin <= r2.end && r2.begin <= r1.end;
}

bool try_fit_in_node(Node* node, RGResourceLifetime* resource)
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
        bool fits_in_children = try_fit_in_node(child, resource);
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

void update_offset(Node* node, uint32_t to_add)
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

        for (uint32_t i = 1; i < local_resources.size(); ++i)
        {
            bool did_fit = try_fit_in_node(parent, local_resources[i]);
            if (did_fit)
            {
                aliased_resources.insert(local_resources[i]);
            }
        }

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

    uint32_t offset = 0;
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

    for (Node* node : buckets)
    {
        delete_node(node);
    }

    return total_size;
}

} // namespace Mizu