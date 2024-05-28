#include "tests_common.h"

static std::shared_ptr<Mizu::Framebuffer> create_test_framebuffer() {
    Mizu::ImageDescription color_desc{};
    color_desc.width = 400;
    color_desc.height = 400;
    color_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
    color_desc.usage = Mizu::ImageUsageBits::Attachment | Mizu::ImageUsageBits::Sampled;

    const auto color_texture = Mizu::Texture2D::create(color_desc);

    Mizu::ImageDescription depth_desc{};
    depth_desc.width = 400;
    depth_desc.height = 400;
    depth_desc.format = Mizu::ImageFormat::D32_SFLOAT;
    depth_desc.usage = Mizu::ImageUsageBits::Attachment;

    const auto depth_texture = Mizu::Texture2D::create(depth_desc);

    Mizu::Framebuffer::Attachment color_attachment{};
    color_attachment.image = color_texture;
    color_attachment.load_operation = Mizu::LoadOperation::Clear;
    color_attachment.store_operation = Mizu::StoreOperation::Store;
    color_attachment.clear_value = glm::vec3(0.0f);

    Mizu::Framebuffer::Attachment depth_attachment{};
    depth_attachment.image = depth_texture;
    depth_attachment.load_operation = Mizu::LoadOperation::Clear;
    depth_attachment.store_operation = Mizu::StoreOperation::DontCare;

    Mizu::Framebuffer::Description framebuffer_desc{};
    framebuffer_desc.width = 400;
    framebuffer_desc.height = 400;
    framebuffer_desc.attachments = {color_attachment, depth_attachment};

    auto framebuffer = Mizu::Framebuffer::create(framebuffer_desc);
    REQUIRE(framebuffer != nullptr);

    return framebuffer;
}

TEST_CASE("RenderPass tests", "[RenderPass]") {
    const auto& [api, backend_config] = GENERATE_GRAPHICS_APIS();

    Mizu::RendererConfiguration config{};
    config.graphics_api = api;
    config.backend_specific_config = backend_config;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::Renderer::initialize(config));

    SECTION("Can create RenderPass") {
        Mizu::RenderPass::Description desc{};
        desc.debug_name = "Test";
        desc.target_framebuffer = create_test_framebuffer();

        const auto rp = Mizu::RenderPass::create(desc);
        REQUIRE(rp != nullptr);
    }

    SECTION("Can begin and end RenderPass") {
        const auto command = Mizu::RenderCommandBuffer::create();
        REQUIRE(command != nullptr);

        Mizu::RenderPass::Description desc{};
        desc.debug_name = "Test";
        desc.target_framebuffer = create_test_framebuffer();

        const auto rp = Mizu::RenderPass::create(desc);
        REQUIRE(rp != nullptr);

        auto fence = Mizu::Fence::create();
        fence->wait_for();

        command->begin();
        {
            command->begin_render_pass(rp);
            command->end_render_pass(rp);
        }
        command->end();

        command->submit({.signal_fence = fence});

        fence->wait_for();
    }

    Mizu::Renderer::shutdown();
}
