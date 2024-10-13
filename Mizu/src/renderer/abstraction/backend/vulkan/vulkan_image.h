#pragma once

#include <vulkan/vulkan.h>

#include "renderer/abstraction/image.h"

namespace Mizu::Vulkan {

class VulkanImage : public virtual IImage {
  public:
    struct Description {
        uint32_t width = 1, height = 1, depth = 1;
        ImageFormat format = ImageFormat::RGBA8_SRGB;
        VkImageType type = VK_IMAGE_TYPE_2D;
        ImageUsageBits usage = ImageUsageBits::None;
        VkImageCreateFlags flags = 0;
        uint32_t num_mips = 1;
        uint32_t num_layers = 1;
    };

    void init_resources(const Description& desc, const SamplingOptions& sampling_options);

    VulkanImage(VkImage image, VkImageView view, bool owning = false);
    ~VulkanImage() override;

    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }
    [[nodiscard]] ImageFormat get_format() const override { return m_description.format; }
    [[nodiscard]] ImageUsageBits get_usage() const override { return m_description.usage; }

    [[nodiscard]] uint32_t get_num_mips() const { return m_description.num_mips; }
    [[nodiscard]] uint32_t get_num_layers() const { return m_description.num_layers; }

    void set_current_state(ImageResourceState state) { m_current_state = state; }

    [[nodiscard]] static VkFormat get_image_format(ImageFormat format);
    [[nodiscard]] static VkImageViewType get_image_view_type(VkImageType type);
    [[nodiscard]] static VkFilter get_vulkan_filter(ImageFilter filter);
    [[nodiscard]] static VkSamplerAddressMode get_vulkan_sampler_address_mode(ImageAddressMode mode);
    [[nodiscard]] static VkImageLayout get_vulkan_image_resource_state(ImageResourceState state);

    [[nodiscard]] VkImage get_image_handle() const { return m_image; }
    [[nodiscard]] VkImageView get_image_view() const { return m_image_view; }
    [[nodiscard]] VkSampler get_sampler() const { return m_sampler; }

  protected:
    VulkanImage() = default;

    VkImage m_image{VK_NULL_HANDLE};
    VkDeviceMemory m_memory{VK_NULL_HANDLE};

    VkImageView m_image_view{VK_NULL_HANDLE};
    VkSampler m_sampler{VK_NULL_HANDLE};

    bool m_owns_resources = true;

    Description m_description{};
    SamplingOptions m_sampling_options{};
    ImageResourceState m_current_state{ImageResourceState::Undefined};

    void create_image();
    void create_image_view();
    void create_sampler();
};

} // namespace Mizu::Vulkan
