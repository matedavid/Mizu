#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#if MIZU_USE_IMGUI
#include <Mizu/Extensions/ImGui.h>
#endif
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
        m_camera_controller->set_position({0.0f, 1.0f, 7.0f});
        m_camera_controller->set_rotation({0.0f, glm::radians(0.0f), 0.0f});

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
            dynamic_state.intensity = 0.0f;
            dynamic_state.cast_shadows = true;
            dynamic_state.data.directional = LightDynamicState::Directional{.direction = glm::vec3(1.0f, -1.0f, 0.0f)};

            const LightHandle light_handle = g_light_state_manager->sim_create(static_state, dynamic_state);
            m_light_handles.push_back(light_handle);
        }

        RenderViewDynamicState render_view_ds{};
        render_view_ds.viewport = ViewportRect{};
        render_view_ds.layer = 0;

        m_render_view_handle = g_render_view_state_manager->sim_create(RenderViewStaticState{}, render_view_ds);
    }

    void update(double dt) override
    {
        static double time = 0.0f;
        time += dt;

        m_camera_controller->update(dt);
        sim_set_camera_state(*m_camera_controller);

        RenderViewDynamicState& render_view_ds = g_render_view_state_manager->sim_edit(m_render_view_handle);
        render_view_ds.camera = Camera2{
            .position = m_camera_controller->get_position(),
            .rotation = m_camera_controller->get_rotation(),
            .fov = m_camera_controller->get_fov(),
            .aspect = m_camera_controller->get_aspect_ratio(),
            .znear = m_camera_controller->get_znear(),
            .zfar = m_camera_controller->get_zfar(),
        };

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

        if (time > 15.0f && m_suzanne_handle0.is_valid())
        {
            g_static_mesh_state_manager->sim_destroy(m_suzanne_handle0);
            m_suzanne_handle0 = StaticMeshHandle{};
        }

        RendererSettingsDynamicState renderer_settings_ds{};
        renderer_settings_ds.settings = m_renderer_settings;
        sim_update_renderer_settings(renderer_settings_ds);

#if MIZU_USE_IMGUI
        ImGuiDynamicState state{};
        state.func = std::bind(&ExampleLayer::draw_imgui, this);
        sim_set_imgui_state(state);
#endif
    }

#if MIZU_USE_IMGUI

    void draw_imgui()
    {
        ImGui::Begin("Information");
        {
            draw_imgui_camera_info();
            draw_imgui_renderer_settings();
        }
        ImGui::End();
    }

    void draw_imgui_camera_info()
    {
        ImGui::SeparatorText("Camera");

        const glm::vec3& position = m_camera_controller.get_position();
        ImGui::Text("Position: (%f, %f, %f)", position.x, position.y, position.z);

        const float& speed = m_camera_controller.get_speed();
        ImGui::Text("Speed: %.2f m/s", speed);
    }

    void draw_imgui_renderer_settings()
    {
        ImGui::SeparatorText("Renderer Settings");

        RenderGraphRendererSettings& settings = m_renderer_settings;

        if (ImGui::CollapsingHeader("Shadows"))
        {
            CascadedShadowsSettings& shadows = settings.cascaded_shadows;

            ImGui::InputInt("Resolution", (int*)&shadows.resolution);
            shadows.resolution = std::max(shadows.resolution, CascadedShadowsSettings::MIN_RESOLUTION);

            ImGui::InputInt("Num Cascades", (int*)&shadows.num_cascades);
            shadows.num_cascades = std::clamp(shadows.num_cascades, 1u, CascadedShadowsSettings::MAX_NUM_CASCADES);

            if (ImGui::TreeNode("Split Factors"))
            {
                for (uint32_t i = 0; i < shadows.num_cascades; ++i)
                {
                    const std::string input_name = std::format("{}", i);

                    ImGui::InputFloat(input_name.c_str(), &shadows.cascade_split_factors[i]);
                    shadows.cascade_split_factors[i] = std::clamp(shadows.cascade_split_factors[i], 0.0f, 1.0f);
                }

                ImGui::TreePop();
                ImGui::Spacing();
            }
        }

        if (ImGui::CollapsingHeader("Debug"))
        {
            DebugSettings& debug = settings.debug;

            const char* DEBUG_VIEW_NAMES[] = {"None", "LightCulling", "CascadedShadows"};
            const uint32_t num_views = IM_ARRAYSIZE(DEBUG_VIEW_NAMES);

            uint32_t debug_view_item = static_cast<uint32_t>(debug.view);
            if (ImGui::BeginCombo("Debug View", DEBUG_VIEW_NAMES[debug_view_item]))
            {
                for (uint32_t n = 0; n < num_views; n++)
                {
                    const bool is_selected = debug_view_item == n;
                    if (ImGui::Selectable(DEBUG_VIEW_NAMES[n], is_selected))
                        debug_view_item = n;

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                debug.view = static_cast<DebugSettings::DebugView>(debug_view_item);

                ImGui::EndCombo();
            }
        }
    }
#endif

    void on_window_resized(WindowResizedEvent& event) override
    {
        if (event.get_width() == 0 || event.get_height() == 0)
            return;

        const float aspect_ratio = static_cast<float>(event.get_width()) / static_cast<float>(event.get_height());
        m_camera_controller->set_aspect_ratio(aspect_ratio);
    }

  private:
    std::unique_ptr<EditorCameraController> m_camera_controller;
    RenderGraphRendererSettings m_renderer_settings;

    RenderViewHandle m_render_view_handle;
    StaticMeshHandle m_suzanne_handle0, m_suzanne_handle1;
    std::vector<StaticMeshHandle> m_mesh_handles;
    std::vector<LightHandle> m_light_handles;
};

class SandboxGame : public GameMain
{
  public:
    GameDescription get_game_description() const override
    {
        GameDescription desc{};
        desc.graphics_api = GraphicsApi::Vulkan;
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
