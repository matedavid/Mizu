#include "renderer_tests_common.h"
#include "resources_manager.h"

static std::shared_ptr<Mizu::Framebuffer> get_test_framebuffer() {
    Mizu::ImageDescription texture_desc{};
    texture_desc.width = 100;
    texture_desc.height = 100;
    texture_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
    texture_desc.usage = Mizu::ImageUsageBits::Attachment;

    const auto color_texture = Mizu::Texture2D::create(texture_desc);
    REQUIRE(color_texture != nullptr);

    Mizu::Framebuffer::Attachment color_attachment{};
    color_attachment.image = color_texture;

    Mizu::Framebuffer::Description framebuffer_desc{};
    framebuffer_desc.width = 100;
    framebuffer_desc.height = 100;
    framebuffer_desc.attachments = {color_attachment};

    const auto framebuffer = Mizu::Framebuffer::create(framebuffer_desc);
    REQUIRE(framebuffer != nullptr);

    return framebuffer;
}

// clang-format off
struct TestUniformBuffer {
    glm::vec4 position;
    glm::vec3 direction; float _padding;
};
// clang-format on

TEST_CASE("GraphicsPipeline tests", "[GraphicsPipeline]") {
    const auto& [api, backend_config] = GENERATE_GRAPHICS_APIS();

    Mizu::RendererConfiguration config{};
    config.graphics_api = api;
    config.backend_specific_config = backend_config;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    REQUIRE(Mizu::Renderer::initialize(config));

    const auto vertex_path = ResourcesManager::get_resource_path("GraphicsShader_1.vert.spv");
    const auto fragment_path = ResourcesManager::get_resource_path("GraphicsShader_1.frag.spv");

    auto shader = Mizu::GraphicsShader::create(Mizu::ShaderStageInfo{vertex_path, "main"},
                                               Mizu::ShaderStageInfo{fragment_path, "main"});
    REQUIRE(shader != nullptr);

    SECTION("Can create GraphicsPipeline") {
        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = get_test_framebuffer();

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);
    }

    SECTION("Can bind GraphicsPipeline") {
        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = get_test_framebuffer();

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);

        const auto command = Mizu::RenderCommandBuffer::create();

        command->begin();
        command->bind_pipeline(pipeline);
        command->end();
    }

    SECTION("Can bind resource group") {
        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = get_test_framebuffer();

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);

        const auto texture = Mizu::Texture2D::create({
            .usage = Mizu::ImageUsageBits::Sampled,
        });
        REQUIRE(texture != nullptr);

        const auto ubo = Mizu::UniformBuffer::create<TestUniformBuffer>();
        REQUIRE(ubo != nullptr);

        const auto set_0 = Mizu::ResourceGroup::create();
        REQUIRE(set_0 != nullptr);

        set_0->add_resource("uTexture1", texture);

        const auto set_1 = Mizu::ResourceGroup::create();
        REQUIRE(set_1 != nullptr);

        set_1->add_resource("uTexture2", texture);
        set_1->add_resource("uUniform1", ubo);

        const auto command = Mizu::RenderCommandBuffer::create();

        command->begin();
        {
            command->bind_resource_group(set_0, 0);
            command->bind_resource_group(set_1, 1);

            command->bind_pipeline(pipeline);
        }
        command->end();
    }

    shader = nullptr;

    Mizu::Renderer::shutdown();
}
