#include <catch2/catch_all.hpp>
#include <Mizu/Mizu.h>

static std::shared_ptr<Mizu::Framebuffer> create_test_framebuffer() {
    Mizu::ImageDescription color_desc{};
    color_desc.width = 400;
    color_desc.height = 400;
    color_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
    color_desc.attachment = true;

    const auto color_texture = Mizu::Texture2D::create(color_desc);

    Mizu::ImageDescription depth_desc{};
    depth_desc.width = 400;
    depth_desc.height = 400;
    depth_desc.format = Mizu::ImageFormat::D32_SFLOAT;
    depth_desc.attachment = true;

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

TEST_CASE("Vulkan RenderPass", "[RenderPass]") {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{.graphics = true, .compute = true};

    REQUIRE(Mizu::initialize(config));

    // Create test framebuffer

    SECTION("Can create RenderPass", "[RenderPass]") {
        Mizu::RenderPass::Description desc{};
        desc.debug_name = "Test";
        desc.target_framebuffer = create_test_framebuffer();

        const auto rp = Mizu::RenderPass::create(desc);
        REQUIRE(rp != nullptr);
    }

    SECTION("Can begin and end RenderPass", "[RenderPass]") {
        const auto cb = Mizu::RenderCommandBuffer::create();
        REQUIRE(cb != nullptr);

        Mizu::RenderPass::Description desc{};
        desc.debug_name = "Test";
        desc.target_framebuffer = create_test_framebuffer();

        const auto rp = Mizu::RenderPass::create(desc);
        REQUIRE(rp != nullptr);

        auto fence = Mizu::Fence::create();
        fence->wait_for();

        cb->begin();
        {
            rp->begin(cb);
            rp->end(cb);
        }
        cb->end();

        cb->submit({.signal_fence = fence});

        fence->wait_for();
    }

    Mizu::shutdown();
}
