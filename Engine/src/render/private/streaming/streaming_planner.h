#pragma once

#include <typeindex>
#include <unordered_map>

#include "base/debug/logging.h"

#include "streaming/streaming_manager.h"

namespace Mizu
{

class FrameLinearAllocator;
class RenderGraphBuilder;

class StreamingPlanner
{
  public:
    StreamingPlanner() = default;

    void update(const StreamingContext& context);
    void add_gpu_uploads_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);

    template <StreamingManagerType T>
    void register_streaming_manager(T* manager)
    {
        const std::type_index index = typeid(T);

        if (m_streaming_manager_map.contains(index))
        {
            MIZU_LOG_WARNING("Streaming manager with type '{}' has already been registered", typeid(T).name());
            return;
        }

        m_streaming_manager_map.emplace(index, manager);
    }

    template <StreamingManagerType T>
    T* get_streaming_manager()
    {
        const std::type_index index = typeid(T);

        const auto it = m_streaming_manager_map.find(index);
        return it != m_streaming_manager_map.end() ? static_cast<T*>(it->second) : nullptr;
    }

  private:
    std::unordered_map<std::type_index, IStreamingManager*> m_streaming_manager_map;
};

} // namespace Mizu
