#include "vulkan_framebuffer.h"

#include <utility>

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_image_resource.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

VulkanFramebuffer::VulkanFramebuffer(FramebufferDescription desc) : m_description(std::move(desc))
{
    MIZU_ASSERT(
        !m_description.color_attachments.is_empty() || m_description.depth_stencil_attachment.has_value(),
        "Empty framebuffer not allowed");
    MIZU_ASSERT(
        m_description.width > 0 && m_description.height > 0,
        "Framebuffer width and height must be greater than 0 (width = {}, height = {})",
        m_description.width,
        m_description.height);

    create_render_pass();
    create_framebuffer();
    create_clear_values();
}

VulkanFramebuffer::VulkanFramebuffer(FramebufferDescription desc, VkRenderPass render_pass)
    : m_render_pass(render_pass)
    , m_owns_render_pass(false)
    , m_description(std::move(desc))
{
    MIZU_ASSERT(m_render_pass != VK_NULL_HANDLE, "RenderPass can't be VK_NULL_HANDLE");

    create_framebuffer();
    create_clear_values();
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    vkDestroyFramebuffer(VulkanContext.device->handle(), m_framebuffer, nullptr);
    if (m_owns_render_pass)
    {
        vkDestroyRenderPass(VulkanContext.device->handle(), m_render_pass, nullptr);
    }
}

void VulkanFramebuffer::create_render_pass()
{
    inplace_vector<VkAttachmentDescription, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> attachment_descriptions;

    inplace_vector<VkAttachmentReference, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments;
    std::optional<VkAttachmentReference> depth_stencil_attachment;

    for (const FramebufferAttachment& attachment : m_description.color_attachments)
    {
        const RenderTargetView& rtv = *attachment.rtv;
        MIZU_ASSERT(
            !ImageUtils::is_depth_format(rtv.get_format()),
            "Can't use a rtv with a depth format as a color attachment");

        VkAttachmentDescription attachment_description{};
        attachment_description.format = VulkanImageResource::get_vulkan_image_format(rtv.get_format());
        attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_description.loadOp = get_load_op(attachment.load_operation);
        attachment_description.storeOp = get_store_op(attachment.store_operation);
        attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        attachment_description.initialLayout =
            VulkanImageResource::get_vulkan_image_resource_state(attachment.initial_state);
        attachment_description.finalLayout =
            VulkanImageResource::get_vulkan_image_resource_state(attachment.final_state);

        attachment_descriptions.push_back(attachment_description);

        VkAttachmentReference reference{};
        reference.attachment = static_cast<uint32_t>(attachment_descriptions.size() - 1);
        reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        color_attachments.push_back(reference);
    }

    if (m_description.depth_stencil_attachment.has_value())
    {
        const FramebufferAttachment& attachment = m_description.depth_stencil_attachment.value();
        const RenderTargetView& rtv = *attachment.rtv;
        MIZU_ASSERT(ImageUtils::is_depth_format(rtv.get_format()), "Depth stencil attachment must have a depth format");

        VkAttachmentDescription attachment_description{};
        attachment_description.format = VulkanImageResource::get_vulkan_image_format(rtv.get_format());
        attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_description.loadOp = get_load_op(attachment.load_operation);
        attachment_description.storeOp = get_store_op(attachment.store_operation);
        // TODO: be able to configure stencilLoadOp and stencilStoreOp
        attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        attachment_description.initialLayout =
            VulkanImageResource::get_vulkan_image_resource_state(attachment.initial_state);
        attachment_description.finalLayout =
            VulkanImageResource::get_vulkan_image_resource_state(attachment.final_state);

        attachment_descriptions.push_back(attachment_description);

        VkAttachmentReference reference{};
        reference.attachment = static_cast<uint32_t>(attachment_descriptions.size() - 1);
        reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_stencil_attachment = reference;
    }

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
    subpass_description.pColorAttachments = color_attachments.data();
    subpass_description.pDepthStencilAttachment =
        depth_stencil_attachment.has_value() ? &depth_stencil_attachment.value() : VK_NULL_HANDLE;

    VkSubpassDependency subpass_dependency{};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;

    subpass_dependency.srcStageMask = 0;
    subpass_dependency.dstStageMask = 0;
    if (!color_attachments.is_empty())
    {
        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    if (subpass_description.pDepthStencilAttachment != VK_NULL_HANDLE)
    {
        subpass_dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        subpass_dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }

    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstAccessMask = 0;
    if (!color_attachments.is_empty())
        subpass_dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    if (subpass_description.pDepthStencilAttachment != VK_NULL_HANDLE)
        subpass_dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = static_cast<uint32_t>(attachment_descriptions.size());
    render_pass_create_info.pAttachments = attachment_descriptions.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_description;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &subpass_dependency;

    VK_CHECK(vkCreateRenderPass(VulkanContext.device->handle(), &render_pass_create_info, nullptr, &m_render_pass));
}

void VulkanFramebuffer::create_framebuffer()
{
    std::vector<VkImageView> framebuffer_attachments;
    for (const FramebufferAttachment& attachment : m_description.color_attachments)
    {
        const VulkanRenderTargetView& rtv = dynamic_cast<const VulkanRenderTargetView&>(*attachment.rtv);
        framebuffer_attachments.push_back(rtv.handle());
    }

    if (m_description.depth_stencil_attachment.has_value())
    {
        const VulkanRenderTargetView& rtv =
            dynamic_cast<const VulkanRenderTargetView&>(*m_description.depth_stencil_attachment.value().rtv);
        framebuffer_attachments.push_back(rtv.handle());
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

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_framebuffer, m_description.name);
    }
}

void VulkanFramebuffer::create_clear_values()
{
    for (const FramebufferAttachment& attachment : get_color_attachments())
    {
        VkClearValue clear_value{};

        const glm::vec4& color = attachment.clear_value;
        clear_value.color = {{color.r, color.g, color.b, color.a}};

        m_clear_values.push_back(clear_value);
    }

    const std::optional<const FramebufferAttachment>& depth_stencil_attachment_opt = get_depth_stencil_attachment();
    if (depth_stencil_attachment_opt.has_value())
    {
        const FramebufferAttachment& attachment = *depth_stencil_attachment_opt;

        m_clear_values.push_back(VkClearValue{
            .depthStencil =
                VkClearDepthStencilValue{
                    .depth = attachment.clear_value.r,
                    .stencil = 0,
                },
        });
    }
}

VkAttachmentLoadOp VulkanFramebuffer::get_load_op(LoadOperation op)
{
    switch (op)
    {
    case LoadOperation::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOperation::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp VulkanFramebuffer::get_store_op(StoreOperation op)
{
    switch (op)
    {
    case StoreOperation::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case StoreOperation::DontCare:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::None:
        return VK_ATTACHMENT_STORE_OP_NONE;
    }
}

} // namespace Mizu::Vulkan
