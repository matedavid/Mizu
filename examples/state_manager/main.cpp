#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Mizu.h>

#include <thread>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

using namespace Mizu;

class ExampleLayer : public Layer
{
  public:
    ExampleLayer()
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

        struct Vertex
        {
            glm::vec3 pos;
        };

        const std::vector<Vertex> vertices = {
            // Front face (normal: 0,0,-1)
            {{-1.0f, -1.0f, -1.0f}},
            {{1.0f, -1.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f}},
            {{-1.0f, 1.0f, -1.0f}},

            // Back face (normal: 0,0,1)
            {{1.0f, -1.0f, 1.0f}},
            {{-1.0f, -1.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f}},
            {{1.0f, 1.0f, 1.0f}},

            // Left face (normal: -1,0,0)
            {{-1.0f, -1.0f, 1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f, 1.0f, -1.0f}},
            {{-1.0f, 1.0f, 1.0f}},

            // Right face (normal: 1,0,0)
            {{1.0f, -1.0f, -1.0f}},
            {{1.0f, -1.0f, 1.0f}},
            {{1.0f, 1.0f, 1.0f}},
            {{1.0f, 1.0f, -1.0f}},

            // Top face (normal: 0,1,0)
            {{-1.0f, 1.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f}},
            {{1.0f, 1.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f}},

            // Bottom face (normal: 0,-1,0)
            {{-1.0f, -1.0f, 1.0f}},
            {{1.0f, -1.0f, 1.0f}},
            {{1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
        };

        // clang-format off
        const std::vector<uint32_t> indices = {
            // Front (0-3)
            0, 1, 2,  0, 2, 3,
            // Back (4-7)
            4, 5, 6,  4, 6, 7,
            // Left (8-11)
            8, 9, 10, 8, 10, 11,
            // Right (12-15)
            12, 13, 14, 12, 14, 15,
            // Top (16-19)
            16, 17, 18, 16, 18, 19,
            // Bottom (20-23)
            20, 21, 22, 20, 22, 23
        };
        // clang-format on

        m_cube_vb = VertexBuffer::create(vertices, Renderer::get_allocator());
        m_cube_ib = IndexBuffer::create(indices, Renderer::get_allocator());
    }

    void on_update(double ts) override
    {
        static uint32_t i = 0;

        if (i == 0)
        {
            m_mesh_transform_handle =
                g_transform_state_manager->sim_create_handle(TransformStaticState{}, TransformDynamicState{});

            StaticMeshStaticState static_state{};
            static_state.transform_handle = m_mesh_transform_handle;
            static_state.vb = m_cube_vb;
            static_state.ib = m_cube_ib;

            m_mesh_handle = g_static_mesh_state_manager->sim_create_handle(static_state, StaticMeshDynamicState{});
        }

        if (i == 500)
        {
            TransformDynamicState transform_dyn_state{};
            transform_dyn_state.translation = glm::vec3(0.0f, 4.0f, 0.0f);

            StaticMeshStaticState static_state{};
            static_state.transform_handle =
                g_transform_state_manager->sim_create_handle(TransformStaticState{}, transform_dyn_state);
            static_state.vb = m_cube_vb;
            static_state.ib = m_cube_ib;

            g_static_mesh_state_manager->sim_create_handle(static_state, StaticMeshDynamicState{});
        }

        if (i == 2000)
        {
            g_transform_state_manager->sim_release_handle(m_mesh_transform_handle);
            g_static_mesh_state_manager->sim_release_handle(m_mesh_handle);

            m_mesh_transform_handle = TransformHandle();
            m_mesh_handle = StaticMeshHandle();
        }

        i += 1;

        m_camera_controller.update(ts);
        sim_set_camera_state(m_camera_controller);

        if (m_mesh_transform_handle.is_valid())
        {
            m_mesh_transform_handle->set_translation(glm::vec3(2.0f * glm::cos(m_total_time), 0.0f, 0.0f));
        }

        m_total_time += ts;
    }

    void on_window_resized(WindowResizedEvent& event) override
    {
        const float aspect_ratio = static_cast<float>(event.get_width()) / static_cast<float>(event.get_height());
        m_camera_controller.set_aspect_ratio(aspect_ratio);
    }

  private:
    FirstPersonCameraController m_camera_controller;

    TransformHandle m_mesh_transform_handle;
    StaticMeshHandle m_mesh_handle;

    std::shared_ptr<VertexBuffer> m_cube_vb;
    std::shared_ptr<IndexBuffer> m_cube_ib;

    double m_total_time = 0.0;
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
