#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>

#include <random>

#include "renderer/deferred/deferred_renderer_shaders.h"

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class ExampleLayer : public Mizu::Layer
{
  public:
    ExampleLayer()
    {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera_controller =
            std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 2.0f, 4.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .lateral_movement_speed = 4.0f,
            .longitudinal_movement_speed = 4.0f,
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);

        const auto mesh_path = example_path / "assets/cube.fbx";

        auto loader = Mizu::AssimpLoader::load(mesh_path);
        MIZU_ASSERT(loader.has_value(), "Could not load mesh");

        Mizu::Texture2D::Description desc{};
        desc.dimensions = {1, 1};
        desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        desc.usage = Mizu::ImageUsageBits::Sampled | Mizu::ImageUsageBits::TransferDst;

        const auto albedo = Mizu::Texture2D::create(
            desc, Mizu::SamplingOptions{}, std::vector<uint8_t>({255, 0, 0, 255}), Mizu::Renderer::get_allocator());

        for (uint32_t row = 1; row <= 5; ++row)
        {
            for (uint32_t col = 1; col <= 5; ++col)
            {
                Mizu::Entity entity = m_scene->create_entity();

                const uint8_t metallic_value = static_cast<uint32_t>(255.0f * (row / 5.0f));
                const uint8_t roughness_value = static_cast<uint32_t>(255.0f * (col / 5.0f));

                auto material = std::make_shared<Mizu::Material>(
                    Mizu::ShaderManager::get_shader({"/EngineShaders/Deferred/PBROpaque.vert.spv", "vsMain"},
                                                    {"/EngineShaders/Deferred/PBROpaque.frag.spv", "fsMain"}));

                material->set("albedo", *albedo);
                material->set("metallic",
                              *Mizu::Texture2D::create(desc,
                                                       Mizu::SamplingOptions{},
                                                       std::vector<uint8_t>({metallic_value, 0, 0, 255}),
                                                       Mizu::Renderer::get_allocator()));
                material->set("roughness",
                              *Mizu::Texture2D::create(desc,
                                                       Mizu::SamplingOptions{},
                                                       std::vector<uint8_t>({roughness_value, 0, 0, 255}),
                                                       Mizu::Renderer::get_allocator()));

                [[maybe_unused]] const bool baked = material->bake();
                MIZU_ASSERT(baked, "Failed to bake material");

                entity.add_component(Mizu::MeshRendererComponent{
                    .mesh = loader->get_meshes()[0],
                    .material = material,
                });

                auto& component = entity.get_component<Mizu::TransformComponent>();
                component.position = glm::vec3(col, row, 0.0f);
                component.scale = glm::vec3(0.25f);
            }
        }

        auto light_material = std::make_shared<Mizu::Material>(
            Mizu::ShaderManager::get_shader({"/EngineShaders/Deferred/PBROpaque.vert.spv", "vsMain"},
                                            {"/EngineShaders/Deferred/PBROpaque.frag.spv", "fsMain"}));

        light_material->set("albedo",
                            *Mizu::Texture2D::create(desc,
                                                     Mizu::SamplingOptions{},
                                                     std::vector<uint8_t>({255, 255, 255, 255}),
                                                     Mizu::Renderer::get_allocator()));
        light_material->set(
            "metallic",
            *Mizu::Texture2D::create(
                desc, Mizu::SamplingOptions{}, std::vector<uint8_t>({2, 0, 0, 255}), Mizu::Renderer::get_allocator()));
        light_material->set(
            "roughness",
            *Mizu::Texture2D::create(
                desc, Mizu::SamplingOptions{}, std::vector<uint8_t>({2, 0, 0, 255}), Mizu::Renderer::get_allocator()));

        [[maybe_unused]] const bool baked = light_material->bake();
        MIZU_ASSERT(baked, "Failed to bake material");

        {
            Mizu::Entity light_1 = m_scene->create_entity();
            light_1.get_component<Mizu::TransformComponent>().position = glm::vec3(2.5f, 2.5f, 2.0f);
            light_1.add_component(Mizu::LightComponent{
                .point_light =
                    Mizu::LightComponent::PointLight{
                        .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                        .intensity = 1.0f,
                    },
            });
            light_1.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_1.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });

            Mizu::Entity light_2 = m_scene->create_entity();
            light_2.get_component<Mizu::TransformComponent>().position = glm::vec3(0.0f, 2.5f, 2.0f);
            light_2.add_component(Mizu::LightComponent{
                .point_light =
                    Mizu::LightComponent::PointLight{
                        .color = glm::vec4(1.0f, 1.0, 1.0f, 1.0f),
                        .intensity = 100.0f,
                    },
            });
            light_2.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_2.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });

            Mizu::Entity light_3 = m_scene->create_entity();
            light_3.get_component<Mizu::TransformComponent>().position = glm::vec3(4.0f, 4.0f, 3.0f);
            light_3.add_component(Mizu::LightComponent{
                .point_light =
                    Mizu::LightComponent::PointLight{
                        .color = glm::vec4(1.0, 1.0, 1.0f, 1.0f),
                        .intensity = 10.0f,
                    },
            });
            light_3.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_3.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });
        }

        const auto skybox_path = std::filesystem::path(MIZU_EXAMPLE_PATH) / "assets/skybox";

        Mizu::Cubemap::Faces faces;
        faces.right = (skybox_path / "right.jpg").string();
        faces.left = (skybox_path / "left.jpg").string();
        faces.top = (skybox_path / "top.jpg").string();
        faces.bottom = (skybox_path / "bottom.jpg").string();
        faces.front = (skybox_path / "front.jpg").string();
        faces.back = (skybox_path / "back.jpg").string();

        const auto skybox = Mizu::Cubemap::create(faces, Mizu::SamplingOptions{}, Mizu::Renderer::get_allocator());

        Mizu::SceneConfig scene_config{};
        scene_config.skybox = skybox;

        m_renderer = std::make_unique<Mizu::DeferredRenderer>(m_scene, scene_config, WIDTH, HEIGHT);
        m_presenter =
            Mizu::Presenter::create(Mizu::Application::instance()->get_window(), m_renderer->get_result_texture());
    }

    void on_update(double ts) override
    {
        m_camera_controller->update(ts);

        m_renderer->render(*m_camera_controller);

        m_presenter->present(m_renderer->get_render_semaphore());
    }

    void on_window_resized(Mizu::WindowResizeEvent& event) override
    {
        Mizu::Renderer::wait_idle();
        m_renderer->resize(event.get_width(), event.get_height());
        m_presenter->texture_changed(m_renderer->get_result_texture());
        m_camera_controller->set_aspect_ratio(static_cast<float>(event.get_width())
                                              / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;
    std::shared_ptr<Mizu::Presenter> m_presenter;
    std::unique_ptr<Mizu::ISceneRenderer> m_renderer;
};

int main()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Deferred Renderer Example";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
