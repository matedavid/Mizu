#pragma once

#include <memory>

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
    };

    virtual ~BottomLevelAccelerationStructure() = default;

    static std::shared_ptr<BottomLevelAccelerationStructure> create(const Description& desc);
};

} // namespace Mizu
