#pragma once

#include "renderer/abstraction/compute_pipeline.h"

namespace Mizu::OpenGL {

class OpenGLComputePipeline : public ComputePipeline {
  public:
    OpenGLComputePipeline(const Description& desc);
};

} // namespace Mizu::OpenGL