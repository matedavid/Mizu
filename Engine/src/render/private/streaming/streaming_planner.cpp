#include "streaming/streaming_planner.h"

#include <ranges>
#include <vector>

#include "render/render_graph/render_graph_builder.h"

namespace Mizu
{

void StreamingPlanner::update(const StreamingContext& context)
{
    std::vector<StreamingRequest> requests{};

    for (IStreamingManager* manager : m_streaming_manager_map | std::views::values)
    {
        manager->update(context);
        manager->gather(requests);
    }

    // TODO: For the moment just execute all requests
    for (const StreamingRequest& request : requests)
    {
        StreamingAward award{
            .request = request,
        };

        std::span<StreamingAward> awards{&award, 1};
        request.manager->execute(awards);
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

            for (IStreamingManager* manager : m_streaming_manager_map | std::views::values)
            {
                manager->setup_render_graph_upload_pass(builder, pass);
            }
        },
        [this, &frame_allocator](CommandBuffer& command, const PassData&, const RenderGraphPassResources&) {
            for (IStreamingManager* manager : m_streaming_manager_map | std::views::values)
            {
                manager->record_gpu_uploads(command, frame_allocator);
            }
        });
}

} // namespace Mizu
