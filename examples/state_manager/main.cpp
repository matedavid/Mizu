#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Mizu.h>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

using namespace Mizu;

class ExampleLayer : public Layer
{
  public:
    void on_init() override
    {
        const uint32_t width = Mizu::Application::instance()->get_window()->get_width();
        const uint32_t height = Mizu::Application::instance()->get_window()->get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        m_camera_controller = EditorCameraController(glm::radians(60.0f), aspect_ratio, 0.001f, 300.0f);
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

            const StaticMeshHandle mesh_handle = g_static_mesh_state_manager->sim_create_handle(ss, {});
            m_mesh_handles.push_back(mesh_handle);
        }

        const auto cube_loader_opt =
            AssimpLoader::load(std::filesystem::path(MIZU_EXAMPLE_ASSETS_PATH) / "Models/Cube/glTF/Cube.gltf");
        MIZU_ASSERT(cube_loader_opt, "Error loading mesh");
        const AssimpLoader& cube_loader = *cube_loader_opt;

        const auto cube_mesh = cube_loader.get_meshes()[0];
        const auto cube_material = cube_loader.get_materials()[0];

        /*
        {
            StaticMeshStaticState ss{};
            ss.transform_handle = g_transform_state_manager->sim_create_handle(
                TransformStaticState{},
                TransformDynamicState{
                    .translation = glm::vec3(0.0f, -1.5f, 0.0f),
                    .scale = glm::vec3(10.0f, 0.19f, 10.0f),
                });
            ss.mesh = cube_mesh;
            ss.material = cube_material;

            const StaticMeshHandle mesh_handle = g_static_mesh_state_manager->sim_create_handle(ss, {});
            m_mesh_handles.push_back(mesh_handle);
        }
        */

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
        m_camera_controller.update(ts);
        sim_set_camera_state(m_camera_controller);
    }

    void on_window_resized(WindowResizedEvent& event) override
    {
        const float aspect_ratio = static_cast<float>(event.get_width()) / static_cast<float>(event.get_height());
        m_camera_controller.set_aspect_ratio(aspect_ratio);
    }

  private:
    EditorCameraController m_camera_controller;

    std::vector<StaticMeshHandle> m_mesh_handles;
    std::vector<LightHandle> m_light_handles;
};

Application* Mizu::create_application()
{
    Application::Description desc{};
    desc.graphics_api = GraphicsAPI::Vulkan;
    desc.name = "State Manager Example";
    desc.width = 1920;
    desc.height = 1080;

    Application* app = new Application(desc);
    app->push_layer<ExampleLayer>();

    return app;
}
