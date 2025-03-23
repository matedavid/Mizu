#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Extensions/ImGuiLayer.h>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

class ExampleLayer : public Mizu::ImGuiLayer
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

        {
            const auto helmet_path = example_path / "assets/john_117/scene.gltf";
            const auto helmet_loader = Mizu::AssimpLoader::load(helmet_path);
            MIZU_ASSERT(helmet_loader.has_value(), "Could not load john_117 model");

            MIZU_LOG_INFO(
                "Helmet info: {} {}", helmet_loader->get_meshes().size(), helmet_loader->get_materials().size());

            Mizu::Entity helmet = m_scene->create_entity();
            helmet.add_component<Mizu::MeshRendererComponent>({
                .mesh = helmet_loader->get_meshes()[0],
                .material = helmet_loader->get_materials()[0],
            });

            Mizu::TransformComponent& transform = helmet.get_component<Mizu::TransformComponent>();
            transform.position = glm::vec3(5.0f, 1.5f, 0.0f);
            transform.scale = glm::vec3(0.30f);
            transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        }

        {
            const auto katana_path = example_path / "assets/damascus_steel_katana/scene.gltf";
            const auto katana_loader = Mizu::AssimpLoader::load(katana_path);
            MIZU_ASSERT(katana_loader.has_value(), "Could not laod damascus_steel_katana model");

            MIZU_LOG_INFO(
                "Katana info: {} {}", katana_loader->get_meshes().size(), katana_loader->get_materials().size());

            Mizu::Entity katana = m_scene->create_entity();
            katana.add_component<Mizu::MeshRendererComponent>({
                .mesh = katana_loader->get_meshes()[0],
                .material = katana_loader->get_materials()[0],
            });

            Mizu::TransformComponent& transform = katana.get_component<Mizu::TransformComponent>();
            transform.position = glm::vec3(-7.0f, 2.0f, 0.0f);
            transform.scale = glm::vec3(0.05f);
            transform.rotation = glm::vec3(0.0f, -90.0f, 0.0f);
        }

        Mizu::Texture2D::Description desc{};
        desc.dimensions = {1, 1};
        desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        desc.usage = Mizu::ImageUsageBits::Sampled | Mizu::ImageUsageBits::TransferDst;
        desc.name = "Red Texture";

        const auto albedo =
            Mizu::Texture2D::create(desc, std::vector<uint8_t>({255, 0, 0, 255}), Mizu::Renderer::get_allocator());

        auto material1 = std::make_shared<Mizu::Material>(
            Mizu::ShaderManager::get_shader({"/EngineShaders/deferred/PBROpaque.vert.spv", "vsMain"},
                                            {"/EngineShaders/deferred/PBROpaque.frag.spv", "fsMain"}));

        {
            material1->set("albedo", Mizu::ImageResourceView::create(albedo->get_resource()));

            desc.name = "Metallic material 1";
            const auto metallic =
                Mizu::Texture2D::create(desc, std::vector<uint8_t>({255, 0, 0, 255}), Mizu::Renderer::get_allocator());
            material1->set("metallic", Mizu::ImageResourceView::create(metallic->get_resource()));

            desc.name = "Roughness material 1";
            const auto roughness =
                Mizu::Texture2D::create(desc, std::vector<uint8_t>({0, 0, 0, 255}), Mizu::Renderer::get_allocator());
            material1->set("roughness", Mizu::ImageResourceView::create(roughness->get_resource()));

            desc.name = "AO material 1";
            const auto ao = Mizu::Texture2D::create(
                desc, std::vector<uint8_t>({255, 255, 255, 255}), Mizu::Renderer::get_allocator());
            material1->set("ambientOcclusion", Mizu::ImageResourceView::create(ao->get_resource()));

            material1->set("sampler", Mizu::RHIHelpers::get_sampler_state(Mizu::SamplingOptions{}));

            [[maybe_unused]] const bool baked = material1->bake();
            MIZU_ASSERT(baked, "Failed to bake material");
        }

        auto material2 = std::make_shared<Mizu::Material>(
            Mizu::ShaderManager::get_shader({"/EngineShaders/deferred/PBROpaque.vert.spv", "vsMain"},
                                            {"/EngineShaders/deferred/PBROpaque.frag.spv", "fsMain"}));

        {
            material2->set("albedo", Mizu::ImageResourceView::create(albedo->get_resource()));

            desc.name = "Metallic material 2";
            const auto metallic =
                Mizu::Texture2D::create(desc, std::vector<uint8_t>({0, 0, 0, 255}), Mizu::Renderer::get_allocator());
            material2->set("metallic", Mizu::ImageResourceView::create(metallic->get_resource()));

            desc.name = "Roughness material 2";
            const auto roughness =
                Mizu::Texture2D::create(desc, std::vector<uint8_t>({255, 0, 0, 255}), Mizu::Renderer::get_allocator());
            material2->set("roughness", Mizu::ImageResourceView::create(roughness->get_resource()));

            desc.name = "AO material 2";
            const auto ao = Mizu::Texture2D::create(
                desc, std::vector<uint8_t>({255, 255, 255, 255}), Mizu::Renderer::get_allocator());
            material2->set("ambientOcclusion", Mizu::ImageResourceView::create(ao->get_resource()));

            material2->set("sampler", Mizu::RHIHelpers::get_sampler_state(Mizu::SamplingOptions{}));

            [[maybe_unused]] const bool baked = material2->bake();
            MIZU_ASSERT(baked, "Failed to bake material");
        }

        for (uint32_t row = 1; row <= 5; ++row)
        {
            for (uint32_t col = 1; col <= 5; ++col)
            {
                Mizu::Entity entity = m_scene->create_entity();

                /*
                const uint8_t metallic_value = static_cast<uint32_t>(255.0f * (row / 5.0f));
                const uint8_t roughness_value = static_cast<uint32_t>(255.0f * (col / 5.0f));

                auto material = std::make_shared<Mizu::Material>(
                    Mizu::ShaderManager::get_shader({"/EngineShaders/deferred/PBROpaque.vert.spv", "vsMain"},
                                                    {"/EngineShaders/deferred/PBROpaque.frag.spv", "fsMain"}));

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
                */

                entity.add_component(Mizu::MeshRendererComponent{
                    .mesh = loader->get_meshes()[0],
                    .material = row % 2 == 0 ? material1 : material2,
                });

                auto& component = entity.get_component<Mizu::TransformComponent>();
                component.position = glm::vec3(col - 2.5f, row, 0.0f);
                component.scale = glm::vec3(0.25f);
            }
        }

        auto light_material = std::make_shared<Mizu::Material>(
            Mizu::ShaderManager::get_shader({"/EngineShaders/deferred/PBROpaque.vert.spv", "vsMain"},
                                            {"/EngineShaders/deferred/PBROpaque.frag.spv", "fsMain"}));

        desc.name = "Light Albedo";
        const auto light_albedo =
            Mizu::Texture2D::create(desc, std::vector<uint8_t>({255, 255, 255, 255}), Mizu::Renderer::get_allocator());
        light_material->set("albedo", Mizu::ImageResourceView::create(light_albedo->get_resource()));

        desc.name = "Light Metallic";
        const auto light_metallic =
            Mizu::Texture2D::create(desc, std::vector<uint8_t>({2, 0, 0, 255}), Mizu::Renderer::get_allocator());
        light_material->set("metallic", Mizu::ImageResourceView::create(light_metallic->get_resource()));

        desc.name = "Light Roughness";
        const auto light_roughness =
            Mizu::Texture2D::create(desc, std::vector<uint8_t>({2, 0, 0, 255}), Mizu::Renderer::get_allocator());
        light_material->set("roughness", Mizu::ImageResourceView::create(light_roughness->get_resource()));

        light_material->set("sampler", Mizu::RHIHelpers::get_sampler_state(Mizu::SamplingOptions{}));

        desc.name = "Light AO";
        const auto ao =
            Mizu::Texture2D::create(desc, std::vector<uint8_t>({255, 255, 255, 255}), Mizu::Renderer::get_allocator());
        light_material->set("ambientOcclusion", Mizu::ImageResourceView::create(ao->get_resource()));

        [[maybe_unused]] const bool baked = light_material->bake();
        MIZU_ASSERT(baked, "Failed to bake material");

        Mizu::Entity floor = m_scene->create_entity();
        floor.get_component<Mizu::TransformComponent>().scale = glm::vec3(20.0f, 0.25f, 20.0f);
        floor.add_component(Mizu::MeshRendererComponent{
            .mesh = loader->get_meshes()[0],
            .material = light_material,
        });

        // Point lights
        {
            Mizu::Entity light_1 = m_scene->create_entity();
            light_1.get_component<Mizu::TransformComponent>().position = glm::vec3(5.0f, 2.5f, 2.0f);
            light_1.add_component(Mizu::PointLightComponent{
                .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                .intensity = 10.0f,
            });
            light_1.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_1.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });

            Mizu::Entity light_2 = m_scene->create_entity();
            light_2.get_component<Mizu::TransformComponent>().position = glm::vec3(0.0f, 2.5f, 2.0f);
            light_2.add_component(Mizu::PointLightComponent{
                .color = glm::vec4(1.0f, 1.0, 1.0f, 1.0f),
                .intensity = 10.0f,
            });
            light_2.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_2.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });

            Mizu::Entity light_3 = m_scene->create_entity();
            light_3.get_component<Mizu::TransformComponent>().position = glm::vec3(4.0f, 4.0f, 3.0f);
            light_3.add_component(Mizu::PointLightComponent{
                .color = glm::vec4(1.0, 1.0, 1.0f, 1.0f),
                .intensity = 10.0f,
            });
            light_3.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_3.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });
        }

        // Directional lights
        {
            /*Mizu::Entity light_1 = m_scene->create_entity();
            light_1.get_component<Mizu::TransformComponent>().position = glm::vec3(2.0f, 10.0f, 12.0f);
            light_1.get_component<Mizu::TransformComponent>().rotation = glm::vec3(-30.0f, 180.0f, 0.0f);
            light_1.add_component(Mizu::DirectionalLightComponent{
                .color = glm::vec3(1.0f),
                .intensity = 1.0f,
                .cast_shadows = true,
            });*/

            // light_1.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            // light_1.add_component(Mizu::MeshRendererComponent{
            //     .mesh = loader->get_meshes()[0],
            //     .material = light_material,
            // });

            /*
            Mizu::Entity light_2 = m_scene->create_entity();
            light_2.get_component<Mizu::TransformComponent>().position = glm::vec3(10.0f, 2.0f, 0.0f);
            light_2.get_component<Mizu::TransformComponent>().rotation = glm::vec3(-30.0f, -90.0f, 0.0f);
            light_2.add_component(Mizu::DirectionalLightComponent{
                .color = glm::vec3(1.0f),
                .intensity = 1.0f,
                .cast_shadows = true,
            });

            light_2.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_2.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });

            Mizu::Entity light_3 = m_scene->create_entity();
            light_3.get_component<Mizu::TransformComponent>().position = glm::vec3(-10.0f, 5.0f, 10.0f);
            light_3.get_component<Mizu::TransformComponent>().rotation = glm::vec3(-30.0f, 135.0f, 0.0f);
            light_3.add_component(Mizu::DirectionalLightComponent{
                .color = glm::vec3(1.0f),
                .intensity = 1.0f,
                .cast_shadows = true,
            });

            light_3.get_component<Mizu::TransformComponent>().scale = glm::vec3(0.1, 0.1, 0.1);
            light_3.add_component(Mizu::MeshRendererComponent{
                .mesh = loader->get_meshes()[0],
                .material = light_material,
            });
            */
        }

        const auto skybox_path = std::filesystem::path(MIZU_EXAMPLE_PATH) / "assets/skybox";

        Mizu::Cubemap::Faces faces;
        faces.right = (skybox_path / "right.jpg").string();
        faces.left = (skybox_path / "left.jpg").string();
        faces.top = (skybox_path / "top.jpg").string();
        faces.bottom = (skybox_path / "bottom.jpg").string();
        faces.front = (skybox_path / "front.jpg").string();
        faces.back = (skybox_path / "back.jpg").string();

        m_skybox = Mizu::Cubemap::create(faces, Mizu::Renderer::get_allocator());

        Mizu::DeferredRendererConfig scene_config{};
        scene_config.skybox = m_skybox;

        Mizu::Texture2D::Description result_desc{};
        result_desc.dimensions = {WIDTH, HEIGHT};
        result_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        result_desc.usage = Mizu::ImageUsageBits::Attachment | Mizu::ImageUsageBits::Sampled;
        result_desc.name = "Result";
        m_result_texture = Mizu::Texture2D::create(result_desc, Mizu::Renderer::get_allocator());
        m_result_texture_view = Mizu::ImageResourceView::create(m_result_texture->get_resource());

        m_result_texture_id = Mizu::ImGuiImpl::add_texture(*m_result_texture_view);

        m_renderer = std::make_unique<Mizu::DeferredRenderer>(m_scene, scene_config, WIDTH, HEIGHT);
    }

    ~ExampleLayer() override
    {
        Mizu::Renderer::wait_idle();

        Mizu::ImGuiImpl::remove_texture(m_result_texture_id);
    }

    void on_update_impl(double ts) override
    {
        // UI
        ImGui::Begin("Config");
        {
            if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const std::string fps = std::format("fps: {}", m_fps);
                ImGui::Text(fps.c_str());
            }

            if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Use Skybox", &m_use_skybox);
            }

            if (ImGui::CollapsingHeader("Shadows", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::InputInt("Num cascades", (int*)&m_renderer_config.num_cascades);
                m_renderer_config.num_cascades = glm::clamp(m_renderer_config.num_cascades, 1u, 10u);

                ImGui::InputFloat("Split lambda", (float*)&m_renderer_config.cascade_split_lambda);
                m_renderer_config.cascade_split_lambda = glm::clamp(m_renderer_config.cascade_split_lambda, 0.0f, 1.0f);

                ImGui::InputFloat("Z scale factor", (float*)&m_renderer_config.z_scale_factor);
                m_renderer_config.z_scale_factor = glm::clamp(m_renderer_config.z_scale_factor, 1.0f, 10.0f);
            }
        }
        ImGui::End();

        m_renderer_config.skybox = m_use_skybox ? m_skybox : nullptr;

        // Render
        m_renderer->change_config(m_renderer_config);

        m_camera_controller->update(ts);

        m_renderer->render(*m_camera_controller, *m_result_texture);

        Mizu::ImGuiImpl::set_background_image(m_result_texture_id);
        Mizu::ImGuiImpl::present(m_renderer->get_render_semaphore());

        constexpr float FPS_AVERAGE_ALPHA = 0.2f;
        m_fps = FPS_AVERAGE_ALPHA * (1.0f / ts) + (1.0f - FPS_AVERAGE_ALPHA) * m_fps;
    }

    void on_window_resized(Mizu::WindowResizeEvent& event) override
    {
        Mizu::Renderer::wait_idle();
        m_renderer->resize(event.get_width(), event.get_height());

        Mizu::Texture2D::Description desc{};
        desc.dimensions = {event.get_width(), event.get_height()};
        desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        desc.usage = Mizu::ImageUsageBits::Attachment | Mizu::ImageUsageBits::Sampled;
        desc.name = "Result";
        m_result_texture = Mizu::Texture2D::create(desc, Mizu::Renderer::get_allocator());
        m_result_texture_view = Mizu::ImageResourceView::create(m_result_texture->get_resource());

        Mizu::ImGuiImpl::remove_texture(m_result_texture_id);
        m_result_texture_id = Mizu::ImGuiImpl::add_texture(*m_result_texture_view);

        m_camera_controller->set_aspect_ratio(static_cast<float>(event.get_width())
                                              / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;
    std::unique_ptr<Mizu::DeferredRenderer> m_renderer;

    Mizu::DeferredRendererConfig m_renderer_config;
    std::shared_ptr<Mizu::Texture2D> m_result_texture;
    std::shared_ptr<Mizu::ImageResourceView> m_result_texture_view;

    std::shared_ptr<Mizu::Cubemap> m_skybox;
    bool m_use_skybox = true;

    ImTextureID m_result_texture_id;

    float m_fps = 1.0f;
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
