#include "render_graph/render_graph_resource_aliasing2.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/debug/profiling.h"

namespace Mizu
{

struct FreeBlock
{
    uint64_t offset;
    uint64_t size;
};

static inline uint64_t align_up(uint64_t value, uint64_t align)
{
    return (value + align - 1) & ~(align - 1);
}

void render_graph_alias_resources(std::vector<AliasingResource>& resources, uint64_t& out_total_size)
{
    MIZU_PROFILE_SCOPED;

    std::sort(resources.begin(), resources.end(), [](const AliasingResource& a, const AliasingResource& b) {
        if (a.begin != b.begin)
            return a.begin < b.begin;

        return a.size > b.size;
    });

    const auto active_allocations_cmp = [](const AliasingResource& a, const AliasingResource& b) {
        return a.end < b.end;
    };

    std::multiset<AliasingResource, decltype(active_allocations_cmp)> active_allocations{active_allocations_cmp};

    std::vector<FreeBlock> free_blocks{};
    free_blocks.reserve(resources.size());

    uint64_t total_size = 0;

    for (AliasingResource& resource : resources)
    {
        while (!active_allocations.empty() && active_allocations.begin()->end < resource.begin)
        {
            const auto expired_it = active_allocations.begin();
            const AliasingResource& expired = *expired_it;

            FreeBlock free_block = FreeBlock{
                .offset = expired.offset,
                .size = expired.size,
            };

            auto free_blocks_it = std::lower_bound(
                free_blocks.begin(), free_blocks.end(), free_block, [](const FreeBlock& a, const FreeBlock& b) {
                    return a.offset < b.offset;
                });

            // Try merge with next block
            if (free_blocks_it != free_blocks.end() && free_block.offset + free_block.size == free_blocks_it->offset)
            {
                free_block.size += free_blocks_it->size;
                free_blocks_it = free_blocks.erase(free_blocks_it);
            }

            // Try merge with previous block
            if (free_blocks_it != free_blocks.begin())
            {
                const auto prev_it = std::prev(free_blocks_it);

                if (prev_it->offset + prev_it->size == free_block.offset)
                {
                    prev_it->size += free_block.size;
                }
                else
                {
                    free_blocks.insert(free_blocks_it, free_block);
                }
            }
            else
            {
                free_blocks.insert(free_blocks_it, free_block);
            }

            active_allocations.erase(expired);
        }

        bool allocated_from_free_block = false;
        for (auto it = free_blocks.begin(); it != free_blocks.end(); ++it)
        {
            const uint64_t aligned_start = align_up(it->offset, resource.alignment);
            const uint64_t allocation_end = aligned_start + resource.size;
            const uint64_t block_end = it->offset + it->size;

            if (allocation_end <= block_end)
            {
                resource.offset = aligned_start;

                if (allocation_end < block_end)
                {
                    it->offset = allocation_end;
                    it->size = block_end - allocation_end;
                }
                else
                {
                    free_blocks.erase(it);
                }

                allocated_from_free_block = true;
                break;
            }
        }

        if (!allocated_from_free_block)
        {
            const uint64_t aligned_start = align_up(total_size, resource.alignment);
            resource.offset = aligned_start;
            total_size += aligned_start + resource.size;
        }

        active_allocations.insert(resource);
    }

    out_total_size = total_size;

#define RENDER_GRAPH_RESOURCE_ALIASING_DEBUG 1

#if RENDER_GRAPH_RESOURCE_ALIASING_DEBUG
    std::vector<AliasingResource> debug_view = resources;
    std::sort(debug_view.begin(), debug_view.end(), [](const AliasingResource& a, const AliasingResource& b) {
        if (a.offset != b.offset)
            return a.offset < b.offset;
        return a.size > b.size;
    });

    MIZU_LOG_INFO("--- Resource Aliasing Map (Total: {:.2f} MB) ---", out_total_size / (1024.0 * 1024.0));

    std::vector<uint64_t> nesting_stack;
    const int base_label_width = 20;

    for (const AliasingResource& res : debug_view)
    {
        const uint64_t res_start = res.offset;
        const uint64_t res_end = res.offset + res.size;

        while (!nesting_stack.empty() && res_start >= nesting_stack.back())
        {
            nesting_stack.pop_back();
        }

        const size_t level = nesting_stack.size();
        const std::string indent_str = std::string(level * 2, ' ');
        const std::string label_str = (level == 0) ? "[Root]" : "[Alias]";

        const std::string full_prefix = indent_str + label_str;

        MIZU_LOG_INFO(
            "{:<{}s} | Offset: {:<10} | Size: {:<10} | Life: {:>3} - {:<3}",
            full_prefix,
            base_label_width,
            res.offset,
            res.size,
            res.begin,
            res.end);
        nesting_stack.push_back(res_end);
    }
    MIZU_LOG_INFO("----------------------------------------------");
#endif
}

} // namespace Mizu