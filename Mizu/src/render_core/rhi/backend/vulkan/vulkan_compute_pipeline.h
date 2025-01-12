#pragma once

#include <string_view>
#include <vulkan/vulkan.h>

#include "render_core/rhi/compute_pipeline.h"

namespace Mizu::Vulkan
{

// Forward declaration
class VulkanComputeShader;

class VulkanComputePipeline : public ComputePipeline
{
  public:
    VulkanComputePipeline(const Description& desc);
    ~VulkanComputePipeline() override;

    void push_constant(VkCommandBuffer command_buffer, std::string_view name, uint32_t size, const void* data)const;

    [[nodiscard]] VkPipeline handle() const { return m_pipeline; }
    [[nodiscard]] std::shared_ptr<VulkanComputeShader> get_shader() const { return m_shader; }

  private:
    VkPipeline m_pipeline;

    std::shared_ptr<VulkanComputeShader> m_shader;
};

} // namespace Mizu::Vulkan