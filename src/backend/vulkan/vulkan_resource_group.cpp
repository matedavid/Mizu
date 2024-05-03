#include "vulkan_resource_group.h"

#include "backend/vulkan/vulkan_buffers.h"
#include "backend/vulkan/vulkan_image.h"
#include "backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) {
    const auto native_texture = std::dynamic_pointer_cast<VulkanTexture2D>(texture);

    VkDescriptorImageInfo info{};
    info.sampler = native_texture->get_sampler();
    info.imageView = native_texture->get_image()->get_image_view();
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    m_image_info.insert({std::string{name}, info});
}

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<VulkanUniformBuffer>(ubo);
    // TODO:
}

} // namespace Mizu::Vulkan