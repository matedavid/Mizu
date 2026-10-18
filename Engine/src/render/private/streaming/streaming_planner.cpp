#include "streaming/streaming_planner.h"

#include <ranges>

#include "render/render_graph/render_graph_builder.h"
#include "render/systems/frame_linear_allocator.h"

namespace Mizu
{

void StreamingPlanner2::update(const StreamingContext& context)
{
    for (IStreamingManager* manager : m_streaming_manager_map | std::views::values)
    {
        manager->update(context);
    }
}

void StreamingPlanner2::add_gpu_uploads_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    (void)frame_allocator;

    struct PassData
    {
    };

    builder.add_pass<PassData>(
        "StreamingPlanner::GpuUploads",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            (void)pass;
            (void)data;
        },
        [](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            (void)command;
            (void)data;
            (void)resources;
        });
}

} // namespace Mizu
