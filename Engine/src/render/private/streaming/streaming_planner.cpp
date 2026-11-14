#include "streaming/streaming_planner.h"

#include <algorithm>

#include "core/runtime.h"

#include "render/render_graph/render_graph_builder.h"
#include "streaming/staging_pool.h"
#include "streaming/streaming_settings.h"

namespace Mizu
{

MIZU_REGISTER_SETTING(StreamingSettings);

StreamingPlanner::StreamingPlanner(StagingPool& staging_pool) : m_staging_pool(staging_pool) {}

StreamingPlanner::~StreamingPlanner()
{
    while (!m_streaming_managers.empty())
    {
        delete m_streaming_managers.back();
        m_streaming_managers.pop_back();
    }
}

void StreamingPlanner::update(const StreamingFrameData& frame_data)
{
    // Copy here so that the reference in StreamingContext comes from this address, instead of the StreamingManager,
    // which could change the values while it's in use
    const StreamingSettings streaming_settings = get_setting<StreamingSettings>();

    const StreamingContext context{
        .frame = frame_data,
        .settings = streaming_settings,
    };

    if (streaming_settings.parallel_update && m_streaming_managers.size() > 1)
    {
        PendingBatch batch = g_job_system->schedule_batch();

        for (size_t i = 1; i < m_streaming_managers.size(); ++i)
        {
            IStreamingManager* manager = m_streaming_managers[i];
            batch.add(JobDescription::create([manager, context]() { manager->update(context); }));
        }

        const JobHandle handle = batch.submit();

        m_streaming_managers[0]->update(context);

        g_job_system->wait_for(handle);
    }
    else
    {
        for (IStreamingManager* manager : m_streaming_managers)
        {
            manager->update(context);
        }
    }

    // TODO: Temporary allocations :(
    std::vector<StreamingRequest> requests{};
    std::vector<size_t> manager_offsets(m_streaming_managers.size() + 1, 0);

    for (uint32_t i = 0; i < m_streaming_managers.size(); ++i)
    {
        const size_t first_request = requests.size();
        m_streaming_managers[i]->gather(requests);

        for (size_t r = first_request; r < requests.size(); ++r)
        {
            requests[r].manager_index = i;
        }

        manager_offsets[i + 1] = requests.size();
    }

    std::sort(requests.begin(), requests.end(), [](const StreamingRequest& a, const StreamingRequest& b) {
        return a.score > b.score;
    });

    std::vector<StreamingAward> awards(requests.size());
    std::vector<size_t> award_cursors(manager_offsets.begin(), manager_offsets.end() - 1);

    for (const StreamingRequest& request : requests)
    {
        StagingBlock staging_block{};

        if (request.staging_size_bytes > 0)
        {
            const std::optional<StagingBlock> staging_alloc = m_staging_pool.allocate(request.staging_size_bytes);
            if (!staging_alloc.has_value())
            {
                continue;
            }

            staging_block = *staging_alloc;
        }

        awards[award_cursors[request.manager_index]++] = StreamingAward{
            .request = request,
            .staging_block = staging_block,
        };
    }

    const std::span<StreamingAward> awards_span{awards};

    for (uint32_t i = 0; i < m_streaming_managers.size(); ++i)
    {
        const size_t first_award = manager_offsets[i];
        const size_t num_awards = award_cursors[i] - first_award;

        if (num_awards > 0)
        {
            m_streaming_managers[i]->execute(awards_span.subspan(first_award, num_awards));
        }
    }

    for (IStreamingManager* manager : m_streaming_managers)
    {
        manager->post_execute();
    }
}

void StreamingPlanner::add_gpu_uploads_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    struct PassData
    {
    };

    builder.add_pass<PassData>(
        "StreamingPlanner::GpuUploads",
        [&](RenderGraphPassBuilder& pass, PassData&) {
            pass.set_hint(RenderGraphPassHint::Transfer);

            for (IStreamingManager* manager : m_streaming_managers)
            {
                manager->setup_render_graph_upload_pass(builder, pass);
            }
        },
        [this, &frame_allocator](CommandBuffer& command, const PassData&, const RenderGraphPassResources&) {
            for (IStreamingManager* manager : m_streaming_managers)
            {
                manager->record_gpu_uploads(command, frame_allocator);
            }
        });
}

} // namespace Mizu
