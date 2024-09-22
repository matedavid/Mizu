#pragma once

#include "renderer/abstraction/compute_pipeline.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLComputeShader;

class OpenGLComputePipeline : public ComputePipeline {
  public:
    OpenGLComputePipeline(const Description& desc);

    void push_constant(std::string_view name, uint32_t size, const void* data);

    [[nodiscard]] std::shared_ptr<OpenGLComputeShader> get_shader() const { return m_shader; }

  private:
    std::shared_ptr<OpenGLComputeShader> m_shader{nullptr};
};

} // namespace Mizu::OpenGL