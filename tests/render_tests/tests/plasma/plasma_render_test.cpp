#include <array>
#include <glm/glm.hpp>

#include "base/debug/assert.h"
#include "render/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/descriptors.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/rhi/sampler_state.h"

#include "render_tests.pipeline/plasma_shaders.h"
#include "runner/render_test.h"
#include "runner/render_tests_registry.h"

using namespace Mizu;

class PlasmaRenderTest : public RenderTest
{
  public:
    std::string_view get_test_group_name() const override { return "Basic"; }
    std::string_view get_test_name() const override { return "Plasma"; }

    void prepare_test(const RenderTestExecutionEnvironment& environment) override
    {
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec2 tex_coords;
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
                Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
                Vertex{{ 3.0f, -1.0f, 0.0f}, {2.0f, 0.0f}},
                Vertex{{-1.0f,  3.0f, 0.0f}, {0.0f, 2.0f}},
            };
            // clang-format on

            const uint8_t* data = reinterpret_cast<const uint8_t*>(vertex_data.data());
            m_vertex_buffer->set_data(data);
        }
        else if (environment.graphics_api == GraphicsApi::Vulkan)
        {
            // clang-format off
            std::array vertex_data = {
                Vertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
                Vertex{{ 3.0f,  1.0f, 0.0f}, {2.0f, 0.0f}},
                Vertex{{-1.0f, -3.0f, 0.0f}, {0.0f, 2.0f}},
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
        const uint32_t width = environment.output_width;
        const uint32_t height = environment.output_height;

        const RenderGraphResource plasma_texture_ref =
            builder.create_texture2d(width, height, ImageFormat::R8G8B8A8_UNORM, "PlasmaTexture");

        struct CreatePlasmaData
        {
            RenderGraphResource output_texture;
        };

        builder.add_pass<CreatePlasmaData>(
            "CreatePlasma",
            [&](RenderGraphPassBuilder& pass, CreatePlasmaData& data) {
                pass.set_hint(RenderGraphPassHint::Compute);
                data.output_texture = pass.write(plasma_texture_ref);
            },
            [=](CommandBuffer& command, const CreatePlasmaData& data, const RenderGraphPassResources& resources) {
                const auto pipeline = get_compute_pipeline(PlasmaShaderCS{});
                command.bind_pipeline(pipeline);

                // clang-format off
                MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(ComputeLayout)
                    MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_UAV(0, 1, ShaderType::Compute)
                MIZU_END_DESCRIPTOR_SET_LAYOUT()
                // clang-format on

                std::array descriptor_set_writes = {
                    WriteDescriptor::TextureUav(0, ImageResourceView::create(resources.get_image(data.output_texture))),
                };

                const auto transient_descriptor_set = g_render_device->allocate_descriptor_set(
                    ComputeLayout::get_layout(), DescriptorSetAllocationType::Transient);
                transient_descriptor_set->update(descriptor_set_writes);

                command.bind_descriptor_set(transient_descriptor_set, 0);

                struct ComputeShaderConstant
                {
                    uint32_t width;
                    uint32_t height;
                    float time;
                };

                const ComputeShaderConstant constant_info{
                    .width = width,
                    .height = height,
                    .time = 1.0f,
                };

                constexpr uint32_t LOCAL_SIZE = 16;
                const auto group_count =
                    glm::uvec3((width + LOCAL_SIZE - 1) / LOCAL_SIZE, (height + LOCAL_SIZE - 1) / LOCAL_SIZE, 1);

                command.push_constant(constant_info);
                command.dispatch(group_count);
            });

        struct DrawPlasmaData
        {
            RenderGraphResource plasma_texture;
            RenderGraphResource output_texture;
        };

        builder.add_pass<DrawPlasmaData>(
            "DrawPlasma",
            [&](RenderGraphPassBuilder& pass, DrawPlasmaData& data) {
                pass.set_hint(RenderGraphPassHint::Raster);

                data.plasma_texture = pass.read(plasma_texture_ref);
                data.output_texture = pass.attachment(environment.output_texture);
            },
            [=, this](CommandBuffer& command, const DrawPlasmaData& data, const RenderGraphPassResources& resources) {
                FramebufferAttachment color_attachment{};
                color_attachment.rtv = ImageResourceView::create(
                    resources.get_image(data.output_texture), {.override_format = ImageFormat::R8G8B8A8_SRGB});
                color_attachment.load_operation = LoadOperation::Clear;
                color_attachment.store_operation = StoreOperation::Store;

                RenderPassInfo pass_info{};
                pass_info.extent = {width, height};
                pass_info.color_attachments = {color_attachment};

                FramebufferInfo framebuffer_info{};
                framebuffer_info.color_attachments = {ImageFormat::R8G8B8A8_SRGB};

                DepthStencilState depth_stencil{};
                depth_stencil.depth_write = false;
                depth_stencil.depth_test = false;

                command.begin_render_pass(pass_info);
                {
                    const auto pipeline = get_graphics_pipeline(
                        PlasmaShaderVS{},
                        PlasmaShaderFS{},
                        RasterizationState{},
                        depth_stencil,
                        ColorBlendState{},
                        framebuffer_info);
                    command.bind_pipeline(pipeline);

                    // clang-format off
                    MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TextureLayout)
                        MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, 1, ShaderType::Fragment)
                        MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)
                    MIZU_END_DESCRIPTOR_SET_LAYOUT()
                    // clang-format on

                    std::array writes = {
                        WriteDescriptor::TextureSrv(
                            0, ImageResourceView::create(resources.get_image(data.plasma_texture))),
                        WriteDescriptor::SamplerState(0, get_sampler_state({})),
                    };

                    const auto transient_descriptor_set = g_render_device->allocate_descriptor_set(
                        TextureLayout::get_layout(), DescriptorSetAllocationType::Transient);
                    transient_descriptor_set->update(writes);

                    command.bind_descriptor_set(transient_descriptor_set, 0);

                    command.bind_vertex_buffer(*m_vertex_buffer);
                    command.draw(3, 0);
                }
                command.end_render_pass();
            });
    }

  private:
    std::shared_ptr<BufferResource> m_vertex_buffer = nullptr;
};

REGISTER_RENDER_TEST(PlasmaRenderTest);
