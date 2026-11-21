#include "runtime/swapchain_manager.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "render_core/rhi/device.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

bool SwapchainManager::init(const SwapchainManagerDescription& desc)
{
    SwapchainDescription swapchain_desc{};
    swapchain_desc.window = desc.window;
    swapchain_desc.format = desc.format;
    swapchain_desc.desired_image_count = desc.desired_image_count;
    swapchain_desc.present_mode = desc.present_mode;

    m_swapchain = g_render_device->create_swapchain(swapchain_desc);
    if (m_swapchain == nullptr)
    {
        MIZU_LOG_ERROR("Failed to create swapchain");
        return false;
    }

    m_image_ready = SyncPoint{};
    m_image_idx = 0;

    return true;
}

void SwapchainManager::acquire_next_image()
{
    MIZU_PROFILE_SCOPED;

    m_image_ready = m_swapchain->acquire_next_image();
    m_image_idx = m_swapchain->get_current_image_idx();
}

void SwapchainManager::present(const SyncPoint& rendering_done)
{
    m_swapchain->present(rendering_done);
}

std::shared_ptr<ImageResource> SwapchainManager::get_current_image() const
{
    return m_swapchain->get_image(m_image_idx);
}

} // namespace Mizu
