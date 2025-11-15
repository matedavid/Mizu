#pragma once

#include <vulkan/vulkan.h>

#include "render_core/rhi/resource_view.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanImageResource;
class VulkanBufferResource;

class VulkanImageResourceView : public ImageResourceView
{
  public:
    VulkanImageResourceView(std::shared_ptr<ImageResource> resource, ImageFormat format, ImageResourceViewRange range);
    ~VulkanImageResourceView() override;

    [[nodiscard]] ImageFormat get_format() const override;
    [[nodiscard]] ImageResourceViewRange get_range() const override { return m_range; }

    // Added because vulkan_resource_group needs to know the usage of the image
    [[nodiscard]] ImageUsageBits get_image_usage() const;

    [[nodiscard]] static VkImageViewType get_vulkan_image_view_type(ImageType type);

    [[nodiscard]] VkImageView handle() const { return m_view; }

  private:
    VkImageView m_view;

    std::shared_ptr<VulkanImageResource> m_resource;
    ImageResourceViewRange m_range;
    ImageFormat m_format;
};

//
// NEW RESOURCE VIEWS FOR D3D12
//

// Forward declarations
class VulkanImageView;

class VulkanShaderResourceView : public ShaderResourceView
{
  public:
    VulkanShaderResourceView(const std::shared_ptr<ImageResource>& resource, ImageResourceViewRange range);
    VulkanShaderResourceView(std::shared_ptr<BufferResource> resource);
    ~VulkanShaderResourceView() override;

    VkImageView get_image_view_handle() const;
    const VulkanBufferResource& get_buffer() const;

  private:
    std::unique_ptr<VulkanImageView> m_image_view = nullptr;
    std::shared_ptr<VulkanBufferResource> m_buffer = nullptr;
};

class VulkanUnorderedAccessView : public UnorderedAccessView
{
  public:
    VulkanUnorderedAccessView(const std::shared_ptr<ImageResource>& resource, ImageResourceViewRange range);
    VulkanUnorderedAccessView(std::shared_ptr<BufferResource> resource);
    ~VulkanUnorderedAccessView() override;

    VkImageView get_image_view_handle() const;
    const VulkanBufferResource& get_buffer() const;

  private:
    std::unique_ptr<VulkanImageView> m_image_view = nullptr;
    std::shared_ptr<VulkanBufferResource> m_buffer = nullptr;
};

class VulkanConstantBufferView : public ConstantBufferView
{
  public:
    VulkanConstantBufferView(std::shared_ptr<BufferResource> resource);

    const VulkanBufferResource& get_buffer() const;

  private:
    std::shared_ptr<VulkanBufferResource> m_buffer = nullptr;
};

class VulkanRenderTargetView : public RenderTargetView
{
  public:
    VulkanRenderTargetView(std::shared_ptr<ImageResource> resource, ImageFormat format, ImageResourceViewRange range);
    ~VulkanRenderTargetView() override;

    VkImageView handle() const;
    ImageFormat get_format() const override;
    ImageResourceViewRange get_range() const override;

  private:
    std::unique_ptr<VulkanImageView> m_image_view = nullptr;
};

} // namespace Mizu::Vulkan