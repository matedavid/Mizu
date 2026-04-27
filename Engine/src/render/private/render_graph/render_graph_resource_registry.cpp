#include "render/render_graph/render_graph_resource_registry.h"

#include "base/debug/profiling.h"
#include "core/runtime.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/image_resource.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

static constexpr size_t MAX_FRAMES_NON_USED_BEFORE_PURGE = 20;

static size_t hash_buffer_desc(const BufferDescription& desc, uint64_t offset)
{
    size_t hash = 0;

    hash_combine(hash, desc.size);
    hash_combine(hash, desc.stride);
    hash_combine(hash, desc.usage);
    hash_combine(hash, desc.sharing_mode);
    hash_combine(hash, desc.queue_families.to_ulong());

    hash_combine(hash, offset);

    return hash;
}

static size_t hash_image_desc(const ImageDescription& desc, uint64_t offset)
{
    size_t hash = 0;

    hash_combine(hash, desc.width);
    hash_combine(hash, desc.height);
    hash_combine(hash, desc.depth);
    hash_combine(hash, desc.type);
    hash_combine(hash, desc.format);
    hash_combine(hash, desc.usage);
    hash_combine(hash, desc.sharing_mode);
    hash_combine(hash, desc.queue_families.to_ulong());
    hash_combine(hash, desc.num_mips);
    hash_combine(hash, desc.num_layers);

    hash_combine(hash, offset);

    return hash;
}

std::shared_ptr<BufferResource> RenderGraphResourceRegistry::create_buffer(
    const BufferDescription& desc,
    uint64_t offset)
{
    const size_t hash = hash_buffer_desc(desc, offset);

    auto it = m_buffer_cache.find(hash);
    if (it == m_buffer_cache.end())
    {
        BufferResourceInfo resource_info{};
        resource_info.frames_non_used = 0;
        resource_info.resource = g_render_device->create_buffer(desc);

        it = m_buffer_cache.insert({hash, resource_info}).first;
    }

    it->second.frames_non_used = 0;

    return it->second.resource;
}

std::shared_ptr<ImageResource> RenderGraphResourceRegistry::create_image(const ImageDescription& desc, uint64_t offset)
{
    const size_t hash = hash_image_desc(desc, offset);

    auto it = m_image_cache.find(hash);
    if (it == m_image_cache.end())
    {
        ImageResourceInfo resource_info{};
        resource_info.frames_non_used = 0;
        resource_info.resource = g_render_device->create_image(desc);

        it = m_image_cache.insert({hash, resource_info}).first;
    }

    it->second.frames_non_used = 0;

    return it->second.resource;
}

template <typename ResourceT>
static void purge_resources(std::unordered_map<size_t, ResourceT>& cache, bool force)
{
    for (auto it = cache.begin(); it != cache.end();)
    {
        ResourceT& info = it->second;

        if (info.frames_non_used > MAX_FRAMES_NON_USED_BEFORE_PURGE || force)
        {
            MIZU_ASSERT(
                info.resource.use_count() == 1,
                "Resource should be owned by the registry and the job when deferred deletion job is created, but it "
                "has {} references",
                info.resource.use_count());

            PendingJob deletion_job = g_job_system->schedule([resource = info.resource]() mutable {
                MIZU_PROFILE_SCOPED_NAME("RenderGraphResourceRegistry::deferred_deletion_job");

                MIZU_ASSERT(
                    resource.use_count() == 1,
                    "Resource should only have one use count but it has {}",
                    resource.use_count());
                resource.reset();
            });

            it = cache.erase(it);

            deletion_job.submit();

            continue;
        }

        info.frames_non_used += 1;
        ++it;
    }
}

void RenderGraphResourceRegistry::purge()
{
    MIZU_PROFILE_SCOPED;

    purge_resources(m_buffer_cache, false);
    purge_resources(m_image_cache, false);
}

void RenderGraphResourceRegistry::reset()
{
    MIZU_PROFILE_SCOPED;

    purge_resources(m_buffer_cache, true);
    purge_resources(m_image_cache, true);
}

} // namespace Mizu