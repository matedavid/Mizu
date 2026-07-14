#include "registries/render_view_registry.h"

#include <glm/gtc/matrix_transform.hpp>

#include "base/debug/assert.h"
#include "render_core/rhi/device.h"

#include "render/runtime/renderer.h"

namespace Mizu
{

static ViewportRect get_clamped_viewport(const ViewportRect& viewport)
{
    return {
        .offset = glm::clamp(viewport.offset, 0.0f, 1.0f),
        .extent = glm::clamp(viewport.extent, 0.0f, 1.0f),
    };
}

static glm::mat4 create_view_matrix(const Camera2& camera)
{
    glm::mat4 view = glm::mat4(1.0f);

    const glm::vec3& position = camera.position;
    const glm::vec3& rotation = camera.rotation;

    view = glm::rotate(view, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
    view = glm::rotate(view, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw
    view = glm::rotate(view, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Roll
    view = glm::translate(view, -position);

    return view;
}

static glm::mat4 create_proj_matrix(const Camera2& camera)
{
    glm::mat4 projection = glm::perspectiveRH_ZO(camera.fov, camera.aspect, camera.znear, camera.zfar);

    if (g_render_device->get_api() == GraphicsApi::Vulkan)
    {
        projection[1][1] *= -1.0f;
    }

    return projection;
}

RenderViewRegistry::RenderViewRegistry()
{
    g_render_view_state_manager->register_rend_consumer(this);
}

RenderViewRegistry::~RenderViewRegistry()
{
    g_render_view_state_manager->unregister_rend_consumer(this);
}

void RenderViewRegistry::rend_on_create(
    RenderViewHandle handle,
    const RenderViewStaticState&,
    const RenderViewDynamicState& ds)
{
    RenderViewRegistryEntry entry{};
    entry.handle = handle;
    entry.viewport = get_clamped_viewport(ds.viewport);
    entry.camera = ds.camera;
    entry.layer = ds.layer;
    entry.view_matrix = create_view_matrix(ds.camera);
    entry.proj_matrix = create_proj_matrix(ds.camera);
    entry.view_proj_matrix = entry.proj_matrix * entry.view_matrix;
    entry.frustum = Frustum::from_view_projection(entry.view_proj_matrix, ds.camera.position);

    m_views.push_back(entry);
}

void RenderViewRegistry::rend_on_update(RenderViewHandle handle, const RenderViewDynamicState& ds)
{
    const auto it = std::find_if(
        m_views.begin(), m_views.end(), [&](const RenderViewRegistryEntry& entry) { return entry.handle == handle; });
    MIZU_ASSERT(it != m_views.end(), "An updated RenderViewHandle should have an entry on the views array");

    RenderViewRegistryEntry& entry = *it;

    entry.viewport = get_clamped_viewport(ds.viewport);
    entry.camera = ds.camera;
    entry.layer = ds.layer;
    entry.view_matrix = create_view_matrix(ds.camera);
    entry.proj_matrix = create_proj_matrix(ds.camera);
    entry.view_proj_matrix = entry.proj_matrix * entry.view_matrix;
    entry.frustum = Frustum::from_view_projection(entry.view_proj_matrix, ds.camera.position);
}

void RenderViewRegistry::rend_on_destroy(RenderViewHandle handle)
{
    const auto it = std::find_if(
        m_views.begin(), m_views.end(), [&](const RenderViewRegistryEntry& entry) { return entry.handle == handle; });

    if (it == m_views.end())
    {
        MIZU_ASSERT(false, "A destroyed RenderViewHandle should have an entry on the views array");
        return;
    }

    m_views.erase(it, it + 1);
}

static RenderViewRegistry* s_render_view_registry = nullptr;

void render_view_registry_init()
{
    MIZU_ASSERT(s_render_view_registry == nullptr, "RenderViewRegistry is already initialized");
    s_render_view_registry = new RenderViewRegistry{};
}

void render_view_registry_shutdown()
{
    delete s_render_view_registry;
    s_render_view_registry = nullptr;
}

RenderViewRegistry& render_view_registry_get()
{
    MIZU_ASSERT(s_render_view_registry != nullptr, "RenderViewRegistry is not initialized");
    return *s_render_view_registry;
}

std::span<const RenderViewRegistryEntry> render_view_registry_get_views()
{
    return render_view_registry_get().get_views();
}

} // namespace Mizu
