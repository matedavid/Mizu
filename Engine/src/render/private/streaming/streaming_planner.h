#pragma once

#include <typeinfo>
#include <utility>
#include <vector>

#include "base/debug/assert.h"

#include "streaming/streaming_manager.h"

namespace Mizu
{

class FrameLinearAllocator;
class RenderGraphBuilder;
class StagingPool;

class StreamingPlanner
{
  public:
    StreamingPlanner(StagingPool& staging_pool);
    ~StreamingPlanner();

    StreamingPlanner(const StreamingPlanner&) = delete;
    StreamingPlanner& operator=(const StreamingPlanner&) = delete;

    void update(const StreamingFrameData& frame_data);
    void add_gpu_uploads_pass(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator);

    template <StreamingManagerType T, typename... Args>
    T& create_streaming_manager(Args&&... args)
    {
        MIZU_VERIFY(
            get_streaming_manager<T>() == nullptr,
            "Streaming manager with type '{}' has already been created",
            typeid(T).name());

        T* manager = new T(std::forward<Args>(args)...);
        m_streaming_managers.push_back(manager);

        return *manager;
    }

    template <StreamingManagerType T>
    T* get_streaming_manager()
    {
        for (IStreamingManager* manager : m_streaming_managers)
        {
            if (typeid(*manager) == typeid(T))
            {
                return static_cast<T*>(manager);
            }
        }

        return nullptr;
    }

  private:
    StagingPool& m_staging_pool;

    std::vector<IStreamingManager*> m_streaming_managers;
};

} // namespace Mizu
