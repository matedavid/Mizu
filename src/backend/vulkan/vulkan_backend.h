#pragma once

#include "renderer.h"

namespace Mizu::Vulkan {

class VulkanBackend : public IBackend {
  public:
    VulkanBackend() = default;
    ~VulkanBackend() override;

    [[nodiscard]] bool initialize(const Configuration& config) override;

    void draw(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const override;
    void draw_indexed(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const override;
};

} // namespace Mizu