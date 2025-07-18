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
    void on_init()
    {
        const uint32_t width = Mizu::Application::instance()->get_window()->get_width();
        const uint32_t height = Mizu::Application::instance()->get_window()->get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        m_camera_controller = FirstPersonCameraController(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller.set_position({0.0f, 0.0f, 7.0f});
        m_camera_controller.set_config(Mizu::FirstPersonCameraController::Config{
            .lateral_movement_speed = 4.0f,
            .longitudinal_movement_speed = 4.0f,
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        const auto loader_opt =
            AssimpLoader::load(std::filesystem::path(MIZU_EXAMPLE_PATH) / "../deferred/assets/john_117/scene.gltf");
        MIZU_ASSERT(loader_opt, "Error loading mesh");
        const AssimpLoader& loader = *loader_opt;

        StaticMeshStaticState static_state{};
        static_state.transform_handle =
            g_transform_state_manager->sim_create_handle({}, TransformDynamicState{.scale = glm::vec3(0.30f)});
        static_state.mesh = loader.get_meshes()[0];
        static_state.material = loader.get_materials()[0];
        m_helmet_handle = g_static_mesh_state_manager->sim_create_handle(static_state, {});

        const std::vector<glm::vec3> point_light_positions = {
            glm::vec3(2.0f, 0.0f, 0.0f),
            glm::vec3(-2.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 4.0f),
            glm::vec3(0.0f, 0.0f, -4.0f),
            glm::vec3(2.0f, 1.0f, 1.0f),
            glm::vec3(-3.0f, -2.0f, 0.0f),
            glm::vec3(-1.0f, 3.0f, 0.0f),
        };

        for (const glm::vec3& pos : point_light_positions)
        {
            LightStaticState static_state{};
            static_state.transform_handle = g_transform_state_manager->sim_create_handle(
                TransformStaticState{}, TransformDynamicState{.translation = pos});

            LightDynamicState dynamic_state{};
            dynamic_state.color = glm::vec3(1.0f, 1.0f, 1.0f);
            dynamic_state.intensity = 10.0f;
            dynamic_state.cast_shadows = false;
            dynamic_state.data = LightDynamicState::Point{.radius = 5.0f};

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
    FirstPersonCameraController m_camera_controller;

    StaticMeshHandle m_helmet_handle;
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
