#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Mizu.h>

#include <format>

#include "base/debug/profiling.h"
#include "runtime/game_main.h"

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

using namespace Mizu;

class SandboxSimulation : public GameSimulation
{
  public:
    void init() override
    {
        MIZU_PROFILE_SCOPED;

        const uint32_t width = g_game_context->get_window().get_width();
        const uint32_t height = g_game_context->get_window().get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        m_camera_controller = std::make_unique<EditorCameraController>(glm::radians(60.0f), aspect_ratio, 0.1f, 300.0f);
        m_camera_controller->set_position({0.0f, 2.0f, -1.0f});
        m_camera_controller->set_rotation({0.0f, glm::radians(90.0f), 0.0f});

        AssetRegistry& asset_registry = g_game_context->get_asset_registry();

        const uint32_t sponza_num_meshes = AssimpLoader::get_num_meshes(
            std::filesystem::path(MIZU_EXAMPLE_ASSETS_PATH) / "Models/Sponza/glTF/Sponza.gltf");

        for (uint32_t i = 0; i < sponza_num_meshes; ++i)
        {
            StaticMeshStaticState static_state{};
            static_state.transform_handle =
                g_transform_state_manager->sim_create({}, TransformDynamicState{.scale = glm::vec3(0.05f)});
            static_state.mesh_handle = asset_registry.get_mesh_handle("shared:Models/Sponza/glTF/Sponza.gltf", i);
            static_state.material_handle =
                asset_registry.get_material_handle("shared:Models/Sponza/glTF/Sponza.gltf", i);

            const StaticMeshHandle mesh_handle = g_static_mesh_state_manager->sim_create(static_state, {});
            m_mesh_handles.push_back(mesh_handle);
        }

        {
            StaticMeshStaticState ss{};
            ss.transform_handle = g_transform_state_manager->sim_create(
                TransformStaticState{}, TransformDynamicState{.translation = glm::vec3(25.0f, 1.0f, 0.0f)});
            ss.mesh_handle = asset_registry.get_mesh_handle("shared:Models/Suzanne/glTF/Suzanne.gltf");
            ss.material_handle = asset_registry.get_material_handle("shared:Models/Suzanne/glTF/Suzanne.gltf", 0);

            m_suzanne_handle0 = g_static_mesh_state_manager->sim_create(ss, {});
            m_mesh_handles.push_back(m_suzanne_handle0);
        }

        {
            StaticMeshStaticState ss{};
            ss.transform_handle = g_transform_state_manager->sim_create(
                TransformStaticState{}, TransformDynamicState{.translation = glm::vec3(25.0f, 1.0f, -4.0f)});
            ss.mesh_handle = asset_registry.get_mesh_handle("shared:Models/Suzanne/glTF/Suzanne.gltf");
            ss.material_handle = asset_registry.get_material_handle("shared:Models/Suzanne/glTF/Suzanne.gltf", 0);

            m_suzanne_handle1 = g_static_mesh_state_manager->sim_create(ss, {});
            m_mesh_handles.push_back(m_suzanne_handle1);
        }

        const std::vector<glm::vec3> point_light_positions = {
            glm::vec3(2.0f, 2.0f, 0.0f),
            glm::vec3(-2.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 4.0f),
            glm::vec3(0.0f, 1.0f, -4.0f),
            glm::vec3(2.0f, 2.0f, 1.0f),
            glm::vec3(-3.0f, 2.0f, 0.0f),
            glm::vec3(-1.0f, 3.0f, 0.0f),
            glm::vec3(-10.0f, 4.0f, 1.0f),
            glm::vec3(10.0f, 4.0f, 1.0f),
            glm::vec3(20.0f, 2.0f, 0.0f),
            glm::vec3(-15.0f, 7.0f, 0.0f),
            glm::vec3(60.0f, 5.0f, 0.0f),
            glm::vec3(60.0f, 7.0f, 0.0f),
        };

        for (const glm::vec3& pos : point_light_positions)
        {
            const TransformHandle transform_handle = g_transform_state_manager->sim_create(
                TransformStaticState{}, TransformDynamicState{.translation = pos, .scale = glm::vec3(0.075f)});

            LightStaticState static_state{};
            static_state.transform_handle = transform_handle;
            static_state.type = LightType::Point;

            LightDynamicState dynamic_state{};
            dynamic_state.color = glm::vec3(1.0f, 1.0f, 1.0f);
            dynamic_state.intensity = 10.0f;
            dynamic_state.cast_shadows = false;
            dynamic_state.data.point = LightDynamicState::Point{.radius = 10.0f};

            const LightHandle light_handle = g_light_state_manager->sim_create(static_state, dynamic_state);
            m_light_handles.push_back(light_handle);

            StaticMeshStaticState static_mesh_state{};
            static_mesh_state.transform_handle = transform_handle;
            static_mesh_state.mesh_handle = asset_registry.get_mesh_handle("shared:Models/Cube/glTF/Cube.gltf");
            static_mesh_state.material_handle =
                asset_registry.get_material_handle("shared:Models/Cube/glTF/Cube.gltf", 0);

            g_static_mesh_state_manager->sim_create(static_mesh_state, {});
        }

        // Create Directional light
        {
            const TransformHandle transform_handle = g_transform_state_manager->sim_create({}, {});

            LightStaticState static_state{};
            static_state.transform_handle = transform_handle;
            static_state.type = LightType::Directional;

            LightDynamicState dynamic_state{};
            dynamic_state.color = glm::vec3(1.0f, 1.0f, 1.0f);
            dynamic_state.intensity = 1.0f;
            dynamic_state.cast_shadows = true;
            dynamic_state.data.directional = LightDynamicState::Directional{.direction = glm::vec3(1.0f, -1.0f, 0.0f)};

            const LightHandle light_handle = g_light_state_manager->sim_create(static_state, dynamic_state);
            m_light_handles.push_back(light_handle);
        }

        RenderViewDynamicState render_view_ds{};
        render_view_ds.viewport = ViewportRect{};
        render_view_ds.layer = 0;
        render_view_ds.camera = m_camera_controller->get_camera();

        m_render_view_handle = g_render_view_state_manager->sim_create(RenderViewStaticState{}, render_view_ds);

        /*
        RenderSettingsLayerDynamicState render_settings_layer_ds{};

        ShadowRenderSettingsOverride& shadows =
            render_settings_layer_ds.override_component<ShadowRenderSettingsOverride>();
        shadows.resolution = 6000;
        shadows.num_cascades = 1;

        m_render_settings_layer_handle1 = g_render_settings_layer_state_manager->sim_create(
            RenderSettingsLayerStaticState{}, render_settings_layer_ds);

        RenderSettingsLayerDynamicState render_settings_layer_ds2{};

        ShadowRenderSettingsOverride& shadows2 =
            render_settings_layer_ds2.override_component<ShadowRenderSettingsOverride>();
        shadows2.resolution = 10000;

        m_render_settings_layer_handle2 = g_render_settings_layer_state_manager->sim_create(
            RenderSettingsLayerStaticState{}, render_settings_layer_ds2);

        {
            RenderSettingsVolumeStaticState volume_ss{};
            volume_ss.transform = g_transform_state_manager->sim_create(
                TransformStaticState{}, TransformDynamicState{.translation = glm::vec3(4.0f, 1.0f, 0.0f)});

            RenderSettingsVolumeDynamicState volume_ds{};
            volume_ds.shape = RenderSettingsVolumeDynamicState::Box{.half_extents = glm::vec3(3.0f)};

            ShadowRenderSettingsOverride& volume_shadows =
                volume_ds.override_component<ShadowRenderSettingsOverride>();
            volume_shadows.resolution = 512;
            volume_shadows.num_cascades = 4;

            m_render_settings_volume_handle =
                g_render_settings_volume_state_manager->sim_create(volume_ss, volume_ds);
        }
        */
    }

    void update(double dt) override
    {
        static double time = 0.0f;
        time += dt;

        m_camera_controller->update(dt);

        static double wait_time = 0.0;
        if (Input::is_key_pressed(Key::G) && wait_time <= 0.0)
        {
            RendererSettings& renderer_settings = get_setting<RendererSettings>();
            renderer_settings.gpu_driven_rendering_enabled = !renderer_settings.gpu_driven_rendering_enabled;

            wait_time += 2.0;
        }

        wait_time = std::max(wait_time - dt, 0.0);

        RenderViewDynamicState& render_view_ds = g_render_view_state_manager->sim_edit(m_render_view_handle);
        render_view_ds.camera = m_camera_controller->get_camera();

        if (Input::is_key_pressed(Key::O) && m_showing_secondary_view && m_secondary_render_view_handle.is_valid())
        {
            RenderViewDynamicState& secondary_render_view_ds =
                g_render_view_state_manager->sim_edit(m_secondary_render_view_handle);
            secondary_render_view_ds.camera = m_camera_controller->get_camera();
            secondary_render_view_ds.camera.position = glm::vec3(21.954311f, 1.2900047f, 0.007057161f);
            secondary_render_view_ds.camera.rotation = glm::vec3(0.021625984f, 1.498166f, 0.0f);
        }
        else if (Input::is_key_pressed(Key::O) && !m_showing_secondary_view)
        {
            // Bottom-right quarter of the screen, pulled in from the right and bottom edges by a small margin.
            constexpr float secondary_view_size = 0.25f;
            constexpr float secondary_view_margin = 0.02f;
            constexpr float secondary_view_offset = 1.0f - secondary_view_size - secondary_view_margin;

            RenderViewDynamicState secondary_render_view_ds{};
            secondary_render_view_ds.viewport = ViewportRect{
                .offset = glm::vec2(secondary_view_offset, secondary_view_offset),
                .extent = glm::vec2(secondary_view_size, secondary_view_size)};
            secondary_render_view_ds.layer = 1;
            secondary_render_view_ds.camera = m_camera_controller->get_camera();
            secondary_render_view_ds.camera.position = glm::vec3(21.954311f, 1.2900047f, 0.007057161f);
            secondary_render_view_ds.camera.rotation = glm::vec3(0.021625984f, 1.498166f, 0.0f);

            m_secondary_render_view_handle =
                g_render_view_state_manager->sim_create(RenderViewStaticState{}, secondary_render_view_ds);

            m_showing_secondary_view = true;
        }
        else if (m_showing_secondary_view)
        {
            g_render_view_state_manager->sim_destroy(m_secondary_render_view_handle);
            m_secondary_render_view_handle = RenderViewHandle{};

            m_showing_secondary_view = false;
        }

        if (m_suzanne_handle0.is_valid())
        {
            const TransformHandle& suzanne_transform_handle =
                g_static_mesh_state_manager->get_static_state(m_suzanne_handle0).transform_handle;

            TransformDynamicState suzanne_ds =
                g_transform_state_manager->sim_get_dynamic_state(suzanne_transform_handle);
            suzanne_ds.rotation.y = glm::radians(static_cast<float>(time * 10.0f));
            g_transform_state_manager->sim_update(suzanne_transform_handle, suzanne_ds);
        }

        if (m_suzanne_handle1.is_valid())
        {
            const TransformHandle& suzanne_transform_handle =
                g_static_mesh_state_manager->get_static_state(m_suzanne_handle1).transform_handle;

            TransformDynamicState suzanne_ds =
                g_transform_state_manager->sim_get_dynamic_state(suzanne_transform_handle);
            suzanne_ds.rotation.y = glm::radians(static_cast<float>(-time * 20.0f));
            g_transform_state_manager->sim_update(suzanne_transform_handle, suzanne_ds);
        }

        if (time > 10.0f && m_suzanne_handle1.is_valid())
        {
            g_static_mesh_state_manager->sim_destroy(m_suzanne_handle1);
            m_suzanne_handle1 = StaticMeshHandle{};
        }

        // if (time > 15.0f && m_suzanne_handle0.is_valid())
        // {
        //     g_static_mesh_state_manager->sim_destroy(m_suzanne_handle0);
        //     m_suzanne_handle0 = StaticMeshHandle{};
        // }
    }

    void on_window_resized(WindowResizedEvent& event) override
    {
        if (event.get_width() == 0 || event.get_height() == 0)
            return;

        const float aspect_ratio = static_cast<float>(event.get_width()) / static_cast<float>(event.get_height());
        m_camera_controller->set_aspect_ratio(aspect_ratio);
    }

  private:
    std::unique_ptr<EditorCameraController> m_camera_controller;

    RenderViewHandle m_render_view_handle;
    RenderViewHandle m_secondary_render_view_handle;
    bool m_showing_secondary_view = false;

    StaticMeshHandle m_suzanne_handle0, m_suzanne_handle1;
    std::vector<StaticMeshHandle> m_mesh_handles;
    std::vector<LightHandle> m_light_handles;

    // RenderSettingsLayerHandle m_render_settings_layer_handle1;
    // RenderSettingsLayerHandle m_render_settings_layer_handle2;
    // RenderSettingsVolumeHandle m_render_settings_volume_handle;
};

class SandboxGame : public GameMain
{
  public:
    GameDescription get_game_description() const override
    {
        GameDescription desc{};
        desc.width = 1920;
        desc.height = 1080;

        return desc;
    }

    GameSimulation* create_game_simulation() const override { return new SandboxSimulation{}; }
};

GameMain* Mizu::create_game_main()
{
    return new SandboxGame{};
}
