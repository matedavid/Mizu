#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

#include "resources_manager.h"

static std::shared_ptr<Mizu::Framebuffer> get_test_framebuffer() {
    Mizu::ImageDescription texture_desc{};
    texture_desc.width = 100;
    texture_desc.height = 100;
    texture_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
    texture_desc.attachment = true;

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

TEST_CASE("Vulkan Graphics Pipeline", "[GraphicsPipeline]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    REQUIRE(Mizu::initialize(config));

    const auto vertex_path = ResourcesManager::get_resource_path("GraphicsShader_1.Vert.spv");
    const auto fragment_path = ResourcesManager::get_resource_path("GraphicsShader_1.Frag.spv");

    auto shader = Mizu::Shader::create(vertex_path, fragment_path);
    REQUIRE(shader != nullptr);

    SECTION("Can create GraphicsPipeline") {
        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = get_test_framebuffer();

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);
    }

    SECTION("Can bake pipeline") {
        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = get_test_framebuffer();

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);

        const auto texture = Mizu::Texture2D::create({});
        pipeline->add_input("uTexture2", texture);

        TestUniformBuffer ubo_data{};
        const auto ubo = Mizu::UniformBuffer::create<TestUniformBuffer>();
        ubo->update(ubo_data);
        pipeline->add_input("uUniform1", ubo);

        REQUIRE(pipeline->bake());
    }

    SECTION("Can't bake pipeline with incomplete inputs") {
        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = get_test_framebuffer();

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);

        REQUIRE(!pipeline->bake());
    }

    SECTION("Can bake and bind pipeline") {
        const auto cb = Mizu::RenderCommandBuffer::create();
        const auto framebuffer = get_test_framebuffer();

        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = framebuffer;

        const auto pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);
        REQUIRE(pipeline != nullptr);

        const auto texture = Mizu::Texture2D::create({});
        pipeline->add_input("uTexture2", texture);

        TestUniformBuffer ubo_data{};
        const auto ubo = Mizu::UniformBuffer::create<TestUniformBuffer>();
        ubo->update(ubo_data);
        pipeline->add_input("uUniform1", ubo);

        REQUIRE(pipeline->bake());

        Mizu::RenderPass::Description render_pass_desc{};
        render_pass_desc.target_framebuffer = framebuffer;

        const auto render_pass = Mizu::RenderPass::create(render_pass_desc);
        REQUIRE(render_pass != nullptr);

        const auto fence = Mizu::Fence::create();
        REQUIRE(fence != nullptr);

        fence->wait_for();

        cb->begin();
        pipeline->bind(cb);
        cb->end();

        cb->submit({.signal_fence = fence});

        fence->wait_for();
    }

    shader = nullptr;

    Mizu::shutdown();
}
