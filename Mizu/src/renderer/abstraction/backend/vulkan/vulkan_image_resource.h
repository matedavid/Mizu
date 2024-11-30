#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/device_memory_allocator.h"
#include "renderer/abstraction/image_resource.h"

namespace Mizu::Vulkan {

class VulkanImageResource : public ImageResource {
  public:
    // NOTE: Should only be used by VulkanTransientImageResource
    VulkanImageResource(const ImageDescription& desc, const SamplingOptions& sampling, bool aliasing);

    VulkanImageResource(const ImageDescription& desc,
                        const SamplingOptions& sampling,
                        std::weak_ptr<IDeviceMemoryAllocator> allocator);

    VulkanImageResource(const ImageDescription& desc,
                        const SamplingOptions& sampling,
                        const std::vector<uint8_t>& content,
                        std::weak_ptr<IDeviceMemoryAllocator> allocator);

    VulkanImageResource(uint32_t width, uint32_t height, VkImage image, VkImageView image_view, bool owns_resources);

    ~VulkanImageResource() override;

    void create_image();
    void create_image_views();
    void create_sampler();

    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }
    [[nodiscard]] uint32_t get_depth() const override { return m_description.depth; }
    [[nodiscard]] ImageType get_image_type() const override { return m_description.type; }
    [[nodiscard]] ImageFormat get_format() const override { return m_description.format; }
    [[nodiscard]] ImageUsageBits get_usage() const override { return m_description.usage; }
    [[nodiscard]] uint32_t get_num_mips() const { return m_description.num_mips; }
    [[nodiscard]] uint32_t get_num_layers() const { return m_description.num_layers; }

    [[nodiscard]] static VkImageType get_image_type(ImageType type);
    [[nodiscard]] static VkFormat get_image_format(ImageFormat format);
    [[nodiscard]] static VkImageViewType get_image_view_type(ImageType type);
    [[nodiscard]] static VkFilter get_vulkan_filter(ImageFilter filter);
    [[nodiscard]] static VkSamplerAddressMode get_vulkan_sampler_address_mode(ImageAddressMode mode);
    [[nodiscard]] static VkImageLayout get_vulkan_image_resource_state(ImageResourceState state);

    [[nodiscard]] uint32_t get_vulkan_usage() const;
    [[nodiscard]] VkImageFormatProperties get_format_properties() const;

    [[nodiscard]] VkImage get_image_handle() const { return m_image; }
    [[nodiscard]] VkImageView get_image_view() const { return m_image_view; }
    [[nodiscard]] VkSampler get_sampler() const { return m_sampler; }

  private:
    VkImage m_image{VK_NULL_HANDLE};
    VkImageView m_image_view{VK_NULL_HANDLE};
    VkSampler m_sampler{VK_NULL_HANDLE};

    std::weak_ptr<IDeviceMemoryAllocator> m_allocator;
    Allocation m_allocation = Allocation::invalid();

    bool m_owns_resources = true;
    bool m_aliased = false;

    ImageDescription m_description;
    SamplingOptions m_sampling_options;
};

} // namespace Mizu::Vulkan