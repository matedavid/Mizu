#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <Mizu/Mizu.h>

#include <cassert>
#include <GLFW/glfw3.h>
#include <iostream>

#include "backend/opengl/opengl_texture.h"

// Mizu Rendering info
struct Vertex {
    glm::vec3 position;
    glm::vec2 tex;
};

static std::vector<Mizu::VertexBuffer::Layout> g_VertexLayout = {
    {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
    {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
};

static std::shared_ptr<Mizu::Texture2D> g_ColorTexture;
static std::shared_ptr<Mizu::Framebuffer> g_ColorFramebuffer;
static std::shared_ptr<Mizu::RenderPass> g_ColorRenderPass;
static std::shared_ptr<Mizu::GraphicsPipeline> g_ColorPipeline;
static std::shared_ptr<Mizu::VertexBuffer> g_VertexBuffer;
static std::shared_ptr<Mizu::IndexBuffer> g_IndexBuffer;

static glm::vec3 g_Color{1.0f, 0.0f, 0.0f};

static std::shared_ptr<Mizu::Texture2D> g_InvertTexture;
static std::shared_ptr<Mizu::Framebuffer> g_InvertFramebuffer;
static std::shared_ptr<Mizu::RenderPass> g_InvertRenderPass;
static std::shared_ptr<Mizu::ResourceGroup> g_InvertResourceGroup;
static std::shared_ptr<Mizu::GraphicsPipeline> g_InvertPipeline;
static std::shared_ptr<Mizu::VertexBuffer> g_FullScreenQuadVertex;
static std::shared_ptr<Mizu::IndexBuffer> g_FullScreenQuadIndex;

static std::shared_ptr<Mizu::UniformBuffer> g_Ubo;

static std::shared_ptr<Mizu::RenderCommandBuffer> g_CommandBuffer;

static std::shared_ptr<Mizu::Fence> g_FlightFence;

static void CreateRenderingInfo(uint32_t width, uint32_t height) {
    g_CommandBuffer = Mizu::RenderCommandBuffer::create();

    {
        Mizu::ImageDescription color_desc{};
        color_desc.width = width;
        color_desc.height = height;
        color_desc.format = Mizu::ImageFormat::BGRA8_SRGB;
        color_desc.attachment = true;

        g_ColorTexture = Mizu::Texture2D::create(color_desc);

        Mizu::Framebuffer::Attachment color_attachment{};
        color_attachment.image = g_ColorTexture;
        color_attachment.load_operation = Mizu::LoadOperation::Clear;
        color_attachment.store_operation = Mizu::StoreOperation::Store;
        color_attachment.clear_value = glm::vec3(0.0f);

        Mizu::Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = width;
        framebuffer_desc.height = height;
        framebuffer_desc.attachments = {color_attachment};

        g_ColorFramebuffer = Mizu::Framebuffer::create(framebuffer_desc);

        const auto shader = Mizu::Shader::create("vertex.spv", "color.frag.spv");

        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = g_ColorFramebuffer;
        pipeline_desc.depth_stencil = Mizu::DepthStencilState{
            .depth_test = false,
            .depth_write = false,
        };

        g_ColorPipeline = Mizu::GraphicsPipeline::create(pipeline_desc);

        Mizu::RenderPass::Description render_pass_desc{};
        render_pass_desc.debug_name = "ColorRenderPass";
        render_pass_desc.target_framebuffer = g_ColorFramebuffer;

        g_ColorRenderPass = Mizu::RenderPass::create(render_pass_desc);

        const std::vector<Vertex> vertex_data = {
            Vertex{.position = glm::vec3{-0.5, 0.5f, 0.0}, .tex = glm::vec2{}},
            Vertex{.position = glm::vec3{0.5, 0.5f, 0.0}, .tex = glm::vec2{}},
            Vertex{.position = glm::vec3{0.0, -0.5f, 0.0}, .tex = glm::vec2{}},
        };
        g_VertexBuffer = Mizu::VertexBuffer::create(vertex_data, g_VertexLayout);

        g_IndexBuffer = Mizu::IndexBuffer::create(std::vector<uint32_t>{0, 1, 2});
    }

    {
        Mizu::ImageDescription texture_desc{};
        texture_desc.width = width;
        texture_desc.height = height;
        texture_desc.format = Mizu::ImageFormat::BGRA8_SRGB;
        texture_desc.attachment = true;

        g_InvertTexture = Mizu::Texture2D::create(texture_desc);

        Mizu::Framebuffer::Attachment color_attachment{};
        color_attachment.image = g_InvertTexture;
        color_attachment.load_operation = Mizu::LoadOperation::Clear;
        color_attachment.store_operation = Mizu::StoreOperation::Store;
        color_attachment.clear_value = glm::vec3(0.0f);

        Mizu::Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = width;
        framebuffer_desc.height = height;
        framebuffer_desc.attachments = {color_attachment};

        g_InvertFramebuffer = Mizu::Framebuffer::create(framebuffer_desc);

        const auto shader = Mizu::Shader::create("vertex.spv", "invert.frag.spv");

        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = g_InvertFramebuffer;
        pipeline_desc.depth_stencil = Mizu::DepthStencilState{
            .depth_test = false,
            .depth_write = false,
        };

        struct InvertUBO {
            glm::vec4 attenuation;
        };
        auto uboData = InvertUBO{.attenuation = glm::vec4(1.0f, 0.0, 1.0f, 1.0f)};

        g_Ubo = Mizu::UniformBuffer::create<InvertUBO>();
        g_Ubo->update(uboData);

        g_InvertResourceGroup = Mizu::ResourceGroup::create();
        g_InvertResourceGroup->add_resource("uColorTexture", g_ColorTexture);
        g_InvertResourceGroup->add_resource("uAttenuationInfo", g_Ubo);

        g_InvertPipeline = Mizu::GraphicsPipeline::create(pipeline_desc);

        //        g_InvertPipeline->add_input(uColorTexture", g_ColorTexture);
        //        assert(g_InvertPipeline->bake());

        Mizu::RenderPass::Description render_pass_desc{};
        render_pass_desc.debug_name = "InvertRenderPass";
        render_pass_desc.target_framebuffer = g_InvertFramebuffer;

        g_InvertRenderPass = Mizu::RenderPass::create(render_pass_desc);

        const std::vector<Vertex> vertex_data = {
            Vertex{.position = glm::vec3{-1.0f, -1.0f, 0.0f}, .tex = glm::vec2{0.0f, 0.0f}},
            Vertex{.position = glm::vec3{1.0f, -1.0f, 0.0f}, .tex = glm::vec2{1.0f, 0.0f}},
            Vertex{.position = glm::vec3{-1.0f, 1.0f, 0.0f}, .tex = glm::vec2{0.0f, 1.0f}},
            Vertex{.position = glm::vec3{1.0f, 1.0f, 0.0f}, .tex = glm::vec2{1.0f, 1.0f}},
        };
        g_FullScreenQuadVertex = Mizu::VertexBuffer::create(vertex_data, g_VertexLayout);
        g_FullScreenQuadIndex = Mizu::IndexBuffer::create(std::vector<uint32_t>{1, 0, 2, 2, 3, 1});
    }

    g_FlightFence = Mizu::Fence::create();
}

static void MizuRenderLogic() {
    g_FlightFence->wait_for();

    g_CommandBuffer->begin();
    {
        // Color pass
        g_CommandBuffer->begin_render_pass(g_ColorRenderPass);

        // clang-format off
        struct ColorInfo {
            glm::vec3 color; float _padding;
        };
        // clang-format on

        g_CommandBuffer->bind_pipeline(g_ColorPipeline);

        g_CommandBuffer->bind_pipeline(g_ColorPipeline);
        assert(g_ColorPipeline->push_constant(g_CommandBuffer, "uColorInfo", ColorInfo{.color = g_Color}));
        g_CommandBuffer->draw_indexed(g_VertexBuffer, g_IndexBuffer);

        g_CommandBuffer->end_render_pass(g_ColorRenderPass);

        // Invert pass
        g_CommandBuffer->begin_render_pass(g_InvertRenderPass);

        g_CommandBuffer->bind_pipeline(g_InvertPipeline);
        g_CommandBuffer->bind_resource_group(g_InvertResourceGroup, 1);
        g_CommandBuffer->draw_indexed(g_FullScreenQuadVertex, g_FullScreenQuadIndex);

        g_CommandBuffer->end_render_pass(g_InvertRenderPass);
    }
    g_CommandBuffer->end();

    Mizu::CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = g_FlightFence;

    g_CommandBuffer->submit(submit_info);
}

int main() {
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW\n";
        return 1;
    }

    const char* glsl_version = "#version 450";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    constexpr uint32_t WIDTH = 1280;
    constexpr uint32_t HEIGHT = 720;

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Initialize Mizu
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::OpenGL;
    config.backend_specific_config = Mizu::OpenGLSpecificConfiguration{.create_context = false};

    assert(Mizu::initialize(config));

    CreateRenderingInfo(WIDTH, HEIGHT);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        // Mizu render logic
        MizuRenderLogic();

        // Draw colorImage
        auto openglTexture = std::dynamic_pointer_cast<Mizu::OpenGL::OpenGLTexture2D>(g_InvertTexture);
        ImGui::GetBackgroundDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(openglTexture->handle()), ImVec2(0, 0), ImVec2(WIDTH, HEIGHT));

        // Color picker
        ImGui::Begin("Color Picker");
        ImGui::ColorPicker3("##ColorPicker", &g_Color[0]);
        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(
            clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    Mizu::shutdown();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}