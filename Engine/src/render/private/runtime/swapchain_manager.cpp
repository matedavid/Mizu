#include "runtime/swapchain_manager.h"

#include <array>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "render_core/rhi/device.h"
#include "render_core/rhi/synchronization.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

bool SwapchainManager::init(const SwapchainManagerDescription& desc)
{
    MIZU_ASSERT(desc.frames_in_flight > 0, "SwapchainManager requires at least one frame in flight");

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

    m_image_acquired_semaphores.resize(desc.frames_in_flight);
    for (std::shared_ptr<Semaphore>& semaphore : m_image_acquired_semaphores)
    {
        semaphore = g_render_device->create_semaphore();
    }

    create_render_finished_semaphores();

    m_frame_in_flight_idx = 0;
    m_image_idx = 0;

    return true;
}

void SwapchainManager::acquire_next_image(uint32_t frame_in_flight_idx, const std::shared_ptr<Fence>& frame_fence)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(
        frame_in_flight_idx < m_image_acquired_semaphores.size(),
        "frame_in_flight_idx is bigger than the number of frames in flight ({} >= {})",
        frame_in_flight_idx,
        m_image_acquired_semaphores.size());

    m_frame_in_flight_idx = frame_in_flight_idx;

    frame_fence->wait_for();

    m_swapchain->acquire_next_image(m_image_acquired_semaphores[m_frame_in_flight_idx], nullptr);

    // The swapchain recreates itself internally when the window is resized, and the image count can change with it.
    if (static_cast<uint32_t>(m_render_finished_semaphores.size()) != m_swapchain->get_num_images())
    {
        create_render_finished_semaphores();
    }

    m_image_idx = m_swapchain->get_current_image_idx();
}

void SwapchainManager::present()
{
    std::array wait_semaphores = {m_render_finished_semaphores[m_image_idx]};
    m_swapchain->present(wait_semaphores);
}

std::shared_ptr<ImageResource> SwapchainManager::get_current_image() const
{
    return m_swapchain->get_image(m_image_idx);
}

const std::shared_ptr<Semaphore>& SwapchainManager::get_image_acquired_semaphore() const
{
    return m_image_acquired_semaphores[m_frame_in_flight_idx];
}

const std::shared_ptr<Semaphore>& SwapchainManager::get_render_finished_semaphore() const
{
    return m_render_finished_semaphores[m_image_idx];
}

void SwapchainManager::create_render_finished_semaphores()
{
    const uint32_t num_images = m_swapchain->get_num_images();

    m_render_finished_semaphores.clear();
    m_render_finished_semaphores.resize(num_images);

    for (std::shared_ptr<Semaphore>& semaphore : m_render_finished_semaphores)
    {
        semaphore = g_render_device->create_semaphore();
    }
}

} // namespace Mizu
