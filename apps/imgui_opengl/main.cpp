#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <Mizu/Mizu.h>

#include <cassert>
#include <GLFW/glfw3.h>
#include <iostream>

#include "backend/opengl/opengl_texture.h"

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

    // Mizu objects
    struct Vertex {
        glm::vec3 position;
        glm::vec2 tex;
    };

    const std::vector<Mizu::VertexBuffer::Layout> vertexLayout = {
        {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
        {.type = Mizu::VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
    };

    auto commandBuffer = Mizu::RenderCommandBuffer::create();
    auto inFlightFence = Mizu::Fence::create();

    Mizu::ImageDescription color_desc{};
    color_desc.width = WIDTH;
    color_desc.height = HEIGHT;
    color_desc.format = Mizu::ImageFormat::BGRA8_SRGB;
    color_desc.attachment = true;

    auto colorTexture = Mizu::Texture2D::create(color_desc);

    Mizu::Framebuffer::Attachment color_attachment{};
    color_attachment.image = colorTexture;
    color_attachment.load_operation = Mizu::LoadOperation::Clear;
    color_attachment.store_operation = Mizu::StoreOperation::Store;
    color_attachment.clear_value = glm::vec3(0.0f);

    Mizu::Framebuffer::Description framebuffer_desc{};
    framebuffer_desc.width = WIDTH;
    framebuffer_desc.height = HEIGHT;
    framebuffer_desc.attachments = {color_attachment};

    auto colorFramebuffer = Mizu::Framebuffer::create(framebuffer_desc);

    const auto shader = Mizu::Shader::create("vertex.spv", "color.frag.spv");

    Mizu::GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.shader = shader;
    pipeline_desc.target_framebuffer = colorFramebuffer;
    pipeline_desc.depth_stencil = Mizu::DepthStencilState{
        .depth_test = false,
        .depth_write = false,
    };

    auto colorPipeline = Mizu::GraphicsPipeline::create(pipeline_desc);

    Mizu::RenderPass::Description render_pass_desc{};
    render_pass_desc.debug_name = "ColorRenderPass";
    render_pass_desc.target_framebuffer = colorFramebuffer;

    auto colorRenderPass = Mizu::RenderPass::create(render_pass_desc);

    const std::vector<Vertex> vertex_data = {
        Vertex{.position = glm::vec3{-0.5, 0.5f, 0.0}, .tex = glm::vec2{}},
        Vertex{.position = glm::vec3{0.5, 0.5f, 0.0}, .tex = glm::vec2{}},
        Vertex{.position = glm::vec3{0.0, -0.5f, 0.0}, .tex = glm::vec2{}},
    };
    auto vertexBuffer = Mizu::VertexBuffer::create(vertex_data, vertexLayout);
    auto indexBuffer = Mizu::IndexBuffer::create(std::vector<uint32_t>{0, 1, 2});

    glm::vec3 triangleColor{1.0f, 0.0f, 0.0};

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        // Mizu render logic
        inFlightFence->wait_for();

        commandBuffer->begin();
        {
            // Color pass
            colorRenderPass->begin(commandBuffer);

            // clang-format off
            struct ColorInfo {
                glm::vec3 color; float _padding;
            };
            // clang-format on

            colorPipeline->bind(commandBuffer);
            assert(colorPipeline->push_constant(commandBuffer, "uColorInfo", ColorInfo{.color = triangleColor}));
            Mizu::draw_indexed(commandBuffer, vertexBuffer, indexBuffer);

            colorRenderPass->end(commandBuffer);
        }
        commandBuffer->end();

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = inFlightFence;

        commandBuffer->submit(submit_info);

        // Draw colorImage
        auto openglTexture = std::dynamic_pointer_cast<Mizu::OpenGL::OpenGLTexture2D>(colorTexture);
        ImGui::GetBackgroundDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(openglTexture->handle()), ImVec2(0, 0), ImVec2(WIDTH, HEIGHT));

        // Color picker
        ImGui::Begin("Color Picker");
        ImGui::ColorPicker3("##ColorPicker", &triangleColor[0]);
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