#include "vulkan_framebuffer.h"

#include <algorithm>
#include <ranges>

#include "renderer/abstraction/texture.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

VulkanFramebuffer::VulkanFramebuffer(const Description& desc) : m_description(desc) {
    assert(!m_description.attachments.empty() && "Empty framebuffer not allowed");
    assert(m_description.width > 0 & m_description.height > 0 && "Framebuffer width and height must be greater than 0");

    // Create Render Pass
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> attachment_references;
    for (const auto& attachment : m_description.attachments) {
        const auto& image = attachment.image;

        VkAttachmentDescription attachment_description{};
        attachment_description.format = VulkanImage::get_image_format(image->get_format());
        attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_description.loadOp = get_load_op(attachment.load_operation);
        attachment_description.storeOp = get_store_op(attachment.store_operation);
        attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        // initialLayout
        attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (ImageUtils::is_depth_format(image->get_format()) && attachment.load_operation == LoadOperation::Load)
            attachment_description.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // finalLayout
        // TODO: Revisit these conditions
        if (ImageUtils::is_depth_format(image->get_format())) {
            attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        } else {
            attachment_description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        attachments.push_back(attachment_description);

        VkAttachmentReference reference{};
        reference.attachment = static_cast<uint32_t>(attachments.size() - 1);
        if (ImageUtils::is_depth_format(image->get_format())) {
            reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        } else {
            reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        attachment_references.push_back(reference);
    }

    std::vector<VkAttachmentReference> color_attachments;
    std::ranges::copy_if(attachment_references, std::back_inserter(color_attachments), [](VkAttachmentReference ref) {
        return ref.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    });

    std::vector<VkAttachmentReference> depth_stencil_attachments;
    std::ranges::copy_if(
        attachment_references, std::back_inserter(depth_stencil_attachments), [](VkAttachmentReference ref) {
            return ref.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        });

    assert(depth_stencil_attachments.size() <= 1 && "Can only have 1 depth / stencil attachment");

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
    subpass_description.pColorAttachments = color_attachments.data();
    subpass_description.pDepthStencilAttachment =
        depth_stencil_attachments.size() == 1 ? &depth_stencil_attachments[0] : VK_NULL_HANDLE;

    VkSubpassDependency subpass_dependency{};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;

    subpass_dependency.srcStageMask = 0;
    subpass_dependency.dstStageMask = 0;
    if (!color_attachments.empty()) {
        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    if (subpass_description.pDepthStencilAttachment != VK_NULL_HANDLE) {
        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }

    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstAccessMask = 0;
    if (!color_attachments.empty())
        subpass_dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (subpass_description.pDepthStencilAttachment != VK_NULL_HANDLE)
        subpass_dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_create_info.pAttachments = attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_description;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &subpass_dependency;

    VK_CHECK(vkCreateRenderPass(VulkanContext.device->handle(), &render_pass_create_info, nullptr, &m_render_pass));

    // Create Framebuffer
    std::vector<VkImageView> framebuffer_attachments;
    for (const auto& attachment : m_description.attachments) {
        const auto texture = std::dynamic_pointer_cast<VulkanTexture2D>(attachment.image);
        const auto& image = texture->get_image();

        framebuffer_attachments.push_back(image->get_image_view());

        assert(m_description.width == attachment.image->get_width()
               && m_description.height == attachment.image->get_height()
               && "All attachments to framebuffer must have the same width and height");
    }

    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = m_render_pass;
    framebuffer_create_info.attachmentCount = static_cast<uint32_t>(framebuffer_attachments.size());
    framebuffer_create_info.pAttachments = framebuffer_attachments.data();
    framebuffer_create_info.width = m_description.width;
    framebuffer_create_info.height = m_description.height;
    framebuffer_create_info.layers = 1;

    VK_CHECK(vkCreateFramebuffer(VulkanContext.device->handle(), &framebuffer_create_info, nullptr, &m_framebuffer));
}

VulkanFramebuffer::~VulkanFramebuffer() {
    vkDestroyFramebuffer(VulkanContext.device->handle(), m_framebuffer, nullptr);
    vkDestroyRenderPass(VulkanContext.device->handle(), m_render_pass, nullptr);
}

VkAttachmentLoadOp VulkanFramebuffer::get_load_op(LoadOperation op) {
    switch (op) {
    case LoadOperation::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOperation::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp VulkanFramebuffer::get_store_op(StoreOperation op) {
    switch (op) {
    case StoreOperation::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case StoreOperation::DontCare:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::None:
        return VK_ATTACHMENT_STORE_OP_NONE;
    }
}

} // namespace Mizu::Vulkan
