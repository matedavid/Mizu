#pragma once

#include <memory>
#include <vector>

#include "render_core/resources/buffers.h"

namespace Mizu
{

class BottomLevelAccelerationStructure
{
  public:
    struct Description
    {
        std::shared_ptr<VertexBuffer> vertex_buffer;
        std::shared_ptr<IndexBuffer> index_buffer;

        std::string name;
    };

    virtual ~BottomLevelAccelerationStructure() = default;

    static std::shared_ptr<BottomLevelAccelerationStructure> create(const Description& desc,
                                                                    std::weak_ptr<IDeviceMemoryAllocator> allocator);
};

class TopLevelAccelerationStructure
{
  public:
    struct InstanceData
    {
        std::shared_ptr<BottomLevelAccelerationStructure> blas;
        glm::mat4 transform;
    };

    struct Description
    {
        std::vector<InstanceData> instances;

        std::string name;
    };

    virtual ~TopLevelAccelerationStructure() = default;

    static std::shared_ptr<TopLevelAccelerationStructure> create(const Description& desc,
                                                                 std::weak_ptr<IDeviceMemoryAllocator> allocator);
};

} // namespace Mizu
