#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Extensions/ImGui.h>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

class ExampleLayer : public Mizu::Layer
{
  public:
    ExampleLayer()
    {
        const uint32_t width = Mizu::Application::instance()->get_window()->get_width();
        const uint32_t height = Mizu::Application::instance()->get_window()->get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
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

        create_scene();

        const auto skybox_path = std::filesystem::path(MIZU_EXAMPLE_PATH) / "assets/skybox";

        Mizu::Cubemap::Faces faces;
        faces.right = (skybox_path / "right.jpg").string();
        faces.left = (skybox_path / "left.jpg").string();
        faces.top = (skybox_path / "top.jpg").string();
        faces.bottom = (skybox_path / "bottom.jpg").string();
        faces.front = (skybox_path / "front.jpg").string();
        faces.back = (skybox_path / "back.jpg").string();

        m_environment = Mizu::Environment::create(faces);

        Mizu::DeferredRendererConfig scene_config{};
        scene_config.environment = m_environment;

        m_imgui_presenter = std::make_unique<Mizu::ImGuiPresenter>(Mizu::Application::instance()->get_window());

        m_renderers.resize(MAX_FRAMES_IN_FLIGHT);
        m_fences.resize(MAX_FRAMES_IN_FLIGHT);
        m_image_acquired_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_result_textures.resize(MAX_FRAMES_IN_FLIGHT);
        m_result_image_views.resize(MAX_FRAMES_IN_FLIGHT);
        m_imgui_textures.resize(MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_renderers[i] = std::make_unique<Mizu::DeferredRenderer>(m_scene, scene_config);
            m_fences[i] = Mizu::Fence::create();
            m_image_acquired_semaphores[i] = Mizu::Semaphore::create();
            m_render_finished_semaphores[i] = Mizu::Semaphore::create();

            Mizu::Texture2D::Description texture_desc{};
            texture_desc.dimensions = {width, height};
            texture_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
            texture_desc.usage = Mizu::ImageUsageBits::Attachment | Mizu::ImageUsageBits::Sampled;
            texture_desc.name = std::format("OutputTexture_{}", i);

            const auto result_texture = Mizu::Texture2D::create(texture_desc, Mizu::Renderer::get_allocator());
            const auto result_view = Mizu::ImageResourceView::create(result_texture->get_resource());

            m_result_textures[i] = result_texture;
            m_result_image_views[i] = result_view;

            const ImTextureID imgui_texture = m_imgui_presenter->add_texture(*result_view);
            m_imgui_textures[i] = imgui_texture;
        }
    }

    ~ExampleLayer() override
    {
        Mizu::Renderer::wait_idle();

        for (ImTextureID texture_id : m_imgui_textures)
        {
            m_imgui_presenter->remove_texture(texture_id);
        }
    }

    void on_update(double ts) override
    {
        m_fences[m_current_frame]->wait_for();

        m_imgui_presenter->acquire_next_image(m_image_acquired_semaphores[m_current_frame], nullptr);
        const auto& image = m_result_textures[m_current_frame];

        m_renderer_config.environment = m_use_skybox ? m_environment : nullptr;

        // Render
        m_camera_controller->update(ts);

        m_renderers[m_current_frame]->change_config(m_renderer_config);

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fences[m_current_frame];
        submit_info.signal_semaphore = m_render_finished_semaphores[m_current_frame];
        submit_info.wait_semaphore = m_image_acquired_semaphores[m_current_frame];

        m_renderers[m_current_frame]->render(*m_camera_controller, *image, submit_info);

        draw_imgui();
        m_imgui_presenter->set_background_texture(m_imgui_textures[m_current_frame]);

        m_imgui_presenter->render_imgui_and_present({m_render_finished_semaphores[m_current_frame]});

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        // Fps calculation
        if (m_frame_num % 30 == 0)
        {
            constexpr float FPS_AVERAGE_ALPHA = 0.8f;
            m_fps = FPS_AVERAGE_ALPHA * (1.0f / ts) + (1.0f - FPS_AVERAGE_ALPHA) * m_fps;
        }

        m_frame_num += 1;
    }

    void draw_imgui()
    {
        ImGui::Begin("Config");
        {
            if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("fps: %f", m_fps);
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

            if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Enabled", (bool*)&m_renderer_config.ssao_enabled);

                if (m_renderer_config.ssao_enabled)
                {
                    ImGui::InputFloat("Radius", (float*)&m_renderer_config.ssao_radius);
                    m_renderer_config.ssao_radius = glm::max(m_renderer_config.ssao_radius, 0.0f);

                    ImGui::Checkbox("Blur Enabled", (bool*)&m_renderer_config.ssao_blur_enabled);
                }
            }
        }
        ImGui::End();
    }

    void on_window_resized(Mizu::WindowResizedEvent& event) override
    {
        Mizu::Renderer::wait_idle();

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            Mizu::Texture2D::Description texture_desc{};
            texture_desc.dimensions = {event.get_width(), event.get_height()};
            texture_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
            texture_desc.usage = Mizu::ImageUsageBits::Attachment | Mizu::ImageUsageBits::Sampled;
            texture_desc.name = "OutputTexture";

            const auto result_texture = Mizu::Texture2D::create(texture_desc, Mizu::Renderer::get_allocator());
            const auto result_view = Mizu::ImageResourceView::create(result_texture->get_resource());

            m_result_textures[i] = result_texture;
            m_result_image_views[i] = result_view;

            m_imgui_presenter->remove_texture(m_imgui_textures[i]);

            const ImTextureID imgui_texture = m_imgui_presenter->add_texture(*result_view);
            m_imgui_textures[i] = imgui_texture;
        }

        m_camera_controller->set_aspect_ratio(static_cast<float>(event.get_width())
                                              / static_cast<float>(event.get_height()));
    }

    void create_scene()
    {
        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);

        const auto mesh_path = example_path / "assets/cube.fbx";

        Mizu::Shader::Description vs_desc{};
        vs_desc.path = "/EngineShaders/deferred/PBROpaque.vert.spv";
        vs_desc.entry_point = "vsMain";
        vs_desc.type = Mizu::ShaderType::Vertex;

        Mizu::Shader::Description fs_desc{};
        fs_desc.path = "/EngineShaders/deferred/PBROpaque.frag.spv";
        fs_desc.entry_point = "fsMain";
        fs_desc.type = Mizu::ShaderType::Fragment;

        const auto mat_vertex_shader = Mizu::ShaderManager::get_shader(vs_desc);
        const auto mat_fragment_shader = Mizu::ShaderManager::get_shader(fs_desc);

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
            MIZU_ASSERT(katana_loader.has_value(), "Could not load damascus_steel_katana model");

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

        auto material1 = std::make_shared<Mizu::Material>(mat_vertex_shader, mat_fragment_shader);

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

        auto material2 = std::make_shared<Mizu::Material>(mat_vertex_shader, mat_fragment_shader);

        {
            material2->set("albedo", Mizu::ImageResourceView::create(albedo->get_resource()));

            desc.name = "Metallic material 2";
            const auto metallic =
                Mizu::Texture2D::create(desc, std::vector<uint8_t>({0, 0, 0, 255}), Mizu::Renderer::get_allocator());
            material2->set("metallic", Mizu::ImageResourceView::create(metallic->get_resource()));

            desc.name = "Roughness material 2";
            const auto roughness =
                Mizu::Texture2D::create(desc, std::vector<uint8_t>({0, 255, 0, 255}), Mizu::Renderer::get_allocator());
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

                entity.add_component(Mizu::MeshRendererComponent{
                    .mesh = loader->get_meshes()[0],
                    .material = row % 2 == 0 ? material1 : material2,
                });

                auto& component = entity.get_component<Mizu::TransformComponent>();
                component.position = glm::vec3(col - 2.5f, row, 0.0f);
                component.scale = glm::vec3(0.25f);
            }
        }

        auto light_material = std::make_shared<Mizu::Material>(mat_vertex_shader, mat_fragment_shader);

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
            Mizu::Texture2D::create(desc, std::vector<uint8_t>({0, 255, 0, 255}), Mizu::Renderer::get_allocator());
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

        // Directional lights
        {
            Mizu::Entity light_1 = m_scene->create_entity();
            light_1.get_component<Mizu::TransformComponent>().position = glm::vec3(2.0f, 10.0f, 12.0f);
            light_1.get_component<Mizu::TransformComponent>().rotation = glm::vec3(-50.0f, 180.0f, 0.0f);
            light_1.add_component(Mizu::DirectionalLightComponent{
                .color = glm::vec3(1.0f),
                .intensity = 3.0f,
                .cast_shadows = true,
            });
        }
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;

    Mizu::DeferredRendererConfig m_renderer_config;
    std::vector<std::unique_ptr<Mizu::DeferredRenderer>> m_renderers;

    std::vector<std::shared_ptr<Mizu::Fence>> m_fences;
    std::vector<std::shared_ptr<Mizu::Semaphore>> m_image_acquired_semaphores, m_render_finished_semaphores;

    uint32_t m_current_frame = 0;

    std::unique_ptr<Mizu::ImGuiPresenter> m_imgui_presenter;

    std::vector<std::shared_ptr<Mizu::Texture2D>> m_result_textures;
    std::vector<std::shared_ptr<Mizu::ImageResourceView>> m_result_image_views;
    std::vector<ImTextureID> m_imgui_textures;

    std::shared_ptr<Mizu::Environment> m_environment;
    bool m_use_skybox = true;

    float m_fps = 1.0f;
    uint64_t m_frame_num = 0;
};

int main()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Deferred Renderer Example";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
