#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#if MIZU_USE_IMGUI
#include <Mizu/Extensions/ImGui.h>
#endif
#include <Mizu/Mizu.h>

#include <format>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

using namespace Mizu;

class ExampleLayer : public Layer
{
  public:
    void on_init() override
    {
        const uint32_t width = Application::instance()->get_window()->get_width();
        const uint32_t height = Application::instance()->get_window()->get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        m_camera_controller = EditorCameraController(glm::radians(60.0f), aspect_ratio, 0.1f, 300.0f);
        m_camera_controller.set_position({0.0f, 1.0f, 7.0f});

        const auto sponza_loader_opt =
            AssimpLoader::load(std::filesystem::path(MIZU_EXAMPLE_ASSETS_PATH) / "Models/Sponza/glTF/Sponza.gltf");
        MIZU_ASSERT(sponza_loader_opt, "Error loading mesh");
        const AssimpLoader& sponza_loader = *sponza_loader_opt;

        for (const AssimpLoader::MeshInfo& mesh_info : sponza_loader.get_meshes_info())
        {
            StaticMeshStaticState static_state{};
            static_state.transform_handle =
                g_transform_state_manager->sim_create_handle({}, TransformDynamicState{.scale = glm::vec3(0.05f)});
            static_state.mesh = sponza_loader.get_meshes()[mesh_info.mesh_idx];
            static_state.material = sponza_loader.get_materials()[mesh_info.material_idx];

            const StaticMeshHandle mesh_handle = g_static_mesh_state_manager->sim_create_handle(static_state, {});
            m_mesh_handles.push_back(mesh_handle);
        }

        const auto suzanne_loader_opt =
            AssimpLoader::load(std::filesystem::path(MIZU_EXAMPLE_ASSETS_PATH) / "Models/Suzanne/glTF/Suzanne.gltf");
        MIZU_ASSERT(suzanne_loader_opt, "Error loading mesh");
        const AssimpLoader& suzanne_loader = *suzanne_loader_opt;

        {
            StaticMeshStaticState ss{};
            ss.transform_handle = g_transform_state_manager->sim_create_handle(
                TransformStaticState{}, TransformDynamicState{.translation = glm::vec3(25.0f, 1.0f, 0.0f)});
            ss.mesh = suzanne_loader.get_meshes()[0];
            ss.material = suzanne_loader.get_materials()[0];

            m_suzanne_handle0 = g_static_mesh_state_manager->sim_create_handle(ss, {});
            m_mesh_handles.push_back(m_suzanne_handle0);
        }

        {
            StaticMeshStaticState ss{};
            ss.transform_handle = g_transform_state_manager->sim_create_handle(
                TransformStaticState{}, TransformDynamicState{.translation = glm::vec3(25.0f, 1.0f, -4.0f)});
            ss.mesh = suzanne_loader.get_meshes()[0];
            ss.material = suzanne_loader.get_materials()[0];

            m_suzanne_handle1 = g_static_mesh_state_manager->sim_create_handle(ss, {});
            m_mesh_handles.push_back(m_suzanne_handle1);
        }

        const auto cube_loader_opt =
            AssimpLoader::load(std::filesystem::path(MIZU_EXAMPLE_ASSETS_PATH) / "Models/Cube/glTF/Cube.gltf");
        MIZU_ASSERT(cube_loader_opt, "Error loading mesh");
        const AssimpLoader& cube_loader = *cube_loader_opt;

        const auto cube_mesh = cube_loader.get_meshes()[0];
        const auto cube_material = cube_loader.get_materials()[0];

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
            const TransformHandle transform_handle = g_transform_state_manager->sim_create_handle(
                TransformStaticState{}, TransformDynamicState{.translation = pos, .scale = glm::vec3(0.075f)});

            LightStaticState static_state{};
            static_state.transform_handle = transform_handle;

            LightDynamicState dynamic_state{};
            dynamic_state.color = glm::vec3(1.0f, 1.0f, 1.0f);
            dynamic_state.intensity = 10.0f;
            dynamic_state.cast_shadows = false;
            dynamic_state.data = LightDynamicState::Point{.radius = 10.0f};

            const LightHandle light_handle = g_light_state_manager->sim_create_handle(static_state, dynamic_state);
            m_light_handles.push_back(light_handle);

            StaticMeshStaticState static_mesh_state{};
            static_mesh_state.transform_handle = transform_handle;
            static_mesh_state.mesh = cube_mesh;
            static_mesh_state.material = cube_material;

            g_static_mesh_state_manager->sim_create_handle(static_mesh_state, {});
        }

        // Create Directional light
        {
            const TransformHandle transform_handle = g_transform_state_manager->sim_create_handle({}, {});

            LightStaticState static_state{};
            static_state.transform_handle = transform_handle;

            LightDynamicState dynamic_state{};
            dynamic_state.color = glm::vec3(1.0f, 1.0f, 1.0f);
            dynamic_state.intensity = 5.0f;
            dynamic_state.cast_shadows = true;
            dynamic_state.data = LightDynamicState::Directional{.direction = glm::vec3(1.0f, -1.0f, 0.0f)};

            const LightHandle light_handle = g_light_state_manager->sim_create_handle(static_state, dynamic_state);
            m_light_handles.push_back(light_handle);
        }
    }

    void on_update(double ts) override
    {
        static double time = 0.0f;
        time += ts;

        m_camera_controller.update(ts);
        sim_set_camera_state(m_camera_controller);

        {
            const TransformHandle& suzanne_transform_handle =
                g_static_mesh_state_manager->sim_get_static_state(m_suzanne_handle0).transform_handle;

            TransformDynamicState suzanne_ds = g_transform_state_manager->get_dynamic_state(suzanne_transform_handle);
            suzanne_ds.rotation.y = glm::radians(static_cast<float>(time * 10.0f));
            g_transform_state_manager->sim_update(suzanne_transform_handle, suzanne_ds);
        }

        {
            const TransformHandle& suzanne_transform_handle =
                g_static_mesh_state_manager->sim_get_static_state(m_suzanne_handle1).transform_handle;

            TransformDynamicState suzanne_ds = g_transform_state_manager->get_dynamic_state(suzanne_transform_handle);
            suzanne_ds.rotation.y = glm::radians(static_cast<float>(-time * 20.0f));
            g_transform_state_manager->sim_update(suzanne_transform_handle, suzanne_ds);
        }

        m_renderer_settings.debug.view = DebugSettings::DebugView::LightCulling;

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
        const float aspect_ratio = static_cast<float>(event.get_width()) / static_cast<float>(event.get_height());
        m_camera_controller.set_aspect_ratio(aspect_ratio);
    }

  private:
    EditorCameraController m_camera_controller;
    RenderGraphRendererSettings m_renderer_settings;

    StaticMeshHandle m_suzanne_handle0, m_suzanne_handle1;
    std::vector<StaticMeshHandle> m_mesh_handles;
    std::vector<LightHandle> m_light_handles;
};

Application* Mizu::create_application()
{
    Application::Description desc{};
    desc.graphics_api = GraphicsApi::Vulkan;
    desc.name = "State Manager Example";
    desc.width = 1920;
    desc.height = 1080;

    Application* app = new Application(desc);
    app->push_layer<ExampleLayer>();

    return app;
}
