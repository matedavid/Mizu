#include <array>
#include <glm/glm.hpp>

#include "base/debug/assert.h"
#include "render/systems/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/pipeline.h"

#include "render_tests.pipeline/hello_triangle_shaders.h"
#include "runner/render_test.h"
#include "runner/render_tests_registry.h"

using namespace Mizu;

class HelloTriangleRenderTest : public RenderTest
{
  public:
    std::string_view get_test_group_name() const override { return "Basic"; }
    std::string_view get_test_name() const override { return "HelloTriangle"; }

    void prepare_test(const RenderTestExecutionEnvironment& environment) override
    {
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec2 tex_coords;
            glm::vec3 color;
        };

        BufferDescription buffer_desc{};
        buffer_desc.size = sizeof(Vertex) * 3;
        buffer_desc.stride = sizeof(Vertex);
        buffer_desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::HostVisible;

        m_vertex_buffer = g_render_device->create_buffer(buffer_desc);

        if (environment.graphics_api == GraphicsApi::Dx12)
        {
            // clang-format off
            std::array vertex_data = {
                Vertex{{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
                Vertex{{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                Vertex{{ 0.0f,  0.5f, 0.0f}, {0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            };
            // clang-format on

            const uint8_t* data = reinterpret_cast<const uint8_t*>(vertex_data.data());
            m_vertex_buffer->set_data(data);
        }
        else if (environment.graphics_api == GraphicsApi::Vulkan)
        {
            // clang-format off
            std::array vertex_data = {
                Vertex{{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
                Vertex{{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                Vertex{{ 0.0f, -0.5f, 0.0f}, {0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            };
            // clang-format on

            const uint8_t* data = reinterpret_cast<const uint8_t*>(vertex_data.data());
            m_vertex_buffer->set_data(data);
        }
        else
        {
            MIZU_UNREACHABLE("Unsupported GraphicsApi");
        }
    }

    void cleanup_test() override { m_vertex_buffer = nullptr; }

    void run_test(RenderGraphBuilder& builder, const RenderTestExecutionEnvironment& environment) override
    {
        struct PassData
        {
            RenderGraphResource output_texture;
        };

        builder.add_pass<PassData>(
            "HelloTrianglePass",
            [&](RenderGraphPassBuilder& pass, PassData& data) {
                pass.set_hint(RenderGraphPassHint::Raster);
                data.output_texture = pass.attachment(environment.output_texture);
            },
            [=, this](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
                const auto output_texture = resources.get_image(data.output_texture);

                FramebufferAttachment color_attachment{};
                color_attachment.rtv =
                    ImageResourceView::create(output_texture, {.override_format = ImageFormat::R8G8B8A8_SRGB});
                color_attachment.load_operation = LoadOperation::Clear;
                color_attachment.store_operation = StoreOperation::Store;

                RenderPassInfo pass_info{};
                pass_info.extent = {environment.output_width, environment.output_height};
                pass_info.color_attachments = {color_attachment};

                FramebufferInfo framebuffer_info{};
                framebuffer_info.color_attachments = {ImageFormat::R8G8B8A8_SRGB};

                DepthStencilState depth_stencil{};
                depth_stencil.depth_write = false;
                depth_stencil.depth_test = false;

                command.begin_render_pass(pass_info);
                {
                    HelloTriangleShaderVS vertex_shader{};
                    HelloTriangleShaderFS fragment_shader{};

                    const auto pipeline = get_graphics_pipeline(
                        vertex_shader,
                        fragment_shader,
                        RasterizationState{},
                        depth_stencil,
                        ColorBlendState{},
                        framebuffer_info);

                    command.bind_pipeline(pipeline);

                    command.bind_vertex_buffer(*m_vertex_buffer);
                    command.draw(3, 0);
                }
                command.end_render_pass();
            });
    }

  private:
    std::shared_ptr<BufferResource> m_vertex_buffer = nullptr;
};

REGISTER_RENDER_TEST(HelloTriangleRenderTest);