#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "renderer/abstraction/compute_pipeline.h"

namespace Mizu::OpenGL
{

// Forward declarations
class OpenGLComputeShader;
class OpenGLBufferResource;

class OpenGLComputePipeline : public ComputePipeline
{
  public:
    OpenGLComputePipeline(const Description& desc);

    void set_state() const;

    void push_constant(std::string_view name, uint32_t size, const void* data);

    [[nodiscard]] std::shared_ptr<OpenGLComputeShader> get_shader() const { return m_shader; }

  private:
    std::shared_ptr<OpenGLComputeShader> m_shader{nullptr};

    std::unordered_map<std::string, std::shared_ptr<OpenGLBufferResource>> m_constants;
};

} // namespace Mizu::OpenGL