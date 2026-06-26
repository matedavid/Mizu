#include <cstdint>
#include <format>
#include <span>
#include <stb_image_write.h>
#include <vector>

#include "base/containers/inplace_vector.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/shader_manager.h"
#include "render_core/rhi/device.h"
#include "render_core/rhi/synchronization.h"

#include "runner/render_test.h"
#include "runner/render_tests_registry.h"

using namespace Mizu;

static constexpr size_t MAX_TOTAL_RENDER_TESTS = 2048;

struct RenderTestsInfo
{
    RenderTestEnvironment environment{};
    std::span<RenderTest*> render_tests{};
};

class RenderTestsRunner
{
  public:
    RenderTestsRunner(RenderTestsInfo info) : m_info(std::move(info))
    {
        ApiSpecificConfiguration specific_config{};
        switch (m_info.environment.graphics_api)
        {
        case GraphicsApi::Dx12:
            specific_config = Dx12SpecificConfiguration{};
            break;
        case GraphicsApi::Vulkan:
            specific_config = VulkanSpecificConfiguration{};
            break;
        }

        const DeviceCreationDescription device_desc{
            .api = m_info.environment.graphics_api,
            .specific_config = specific_config,
            .frames_in_flight = 1,
        };

        g_render_device = Device::create(device_desc);

        ShaderManager::get().add_shader_mapping("/RenderTestShaders", MIZU_ENGINE_SHADERS_PATH);
    }

    ~RenderTestsRunner()
    {
        ShaderManager::get().reset();
        PipelineCache::get().reset();

        delete g_render_device;
        Device::free();
    }

    void run_tests() const
    {
        constexpr uint64_t FRAME_ALLOCATOR_SIZE = 64ull * 1024 * 1024; // 64 MB
        FrameLinearAllocator frame_allocator{1, FRAME_ALLOCATOR_SIZE, "RenderTest_FrameAllocator"};

        constexpr uint32_t TEST_WIDTH = 1920;
        constexpr uint32_t TEST_HEIGHT = 1080;
        constexpr ImageFormat TEST_IMAGE_FORMAT = ImageFormat::R8G8B8A8_UNORM;

        BufferDescription readback_buffer_desc{};
        readback_buffer_desc.size = TEST_WIDTH * TEST_HEIGHT * get_image_format_size(TEST_IMAGE_FORMAT);
        readback_buffer_desc.stride = sizeof(uint8_t);
        readback_buffer_desc.usage = BufferUsageBits::TransferDst | BufferUsageBits::HostVisible;
        readback_buffer_desc.name = "RenderTest_ReadbackBuffer";
        const auto image_readback_buffer = g_render_device->create_buffer(readback_buffer_desc);

        const auto fence = g_render_device->create_fence(false);

        RenderGraphResourceRegistry resource_registry{};
        const auto transient_memory_pool =
            g_render_device->create_transient_memory_pool("RenderTest_TransientMemoryPool");

        RenderGraph render_graph{};
        RenderGraphBuilder builder{};

        for (RenderTest* render_test : m_info.render_tests)
        {
            render_graph.reset();
            builder.reset();

            ImageDescription image_desc{};
            image_desc.width = TEST_WIDTH;
            image_desc.height = TEST_HEIGHT;
            image_desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled | ImageUsageBits::TransferSrc;
            image_desc.flags = ImageFlagBits::MutableFormat;
            image_desc.format = TEST_IMAGE_FORMAT;
            image_desc.name = "RenderTest_OutputImage";

            const RenderGraphResource output_texture = builder.create_texture(image_desc);
            const RenderGraphResource readback_buffer = builder.register_external_buffer(
                image_readback_buffer, {BufferResourceState::TransferDst, BufferResourceState::TransferDst});

            const RenderTestExecutionEnvironment execution_environment{
                .graphics_api = m_info.environment.graphics_api,
                .frame_allocator = &frame_allocator,
                .output_width = TEST_WIDTH,
                .output_height = TEST_HEIGHT,
                .output_texture = output_texture,
            };

            execute_test(render_test, builder, execution_environment);
            add_texture_readback_pass(builder, output_texture, readback_buffer);

            const RenderGraphBuilderCompileOptions compile_options{
                *transient_memory_pool,
                resource_registry,
            };

            builder.compile(render_graph, compile_options);

            render_graph.execute({
                .signal_fence = fence,
            });

            fence->wait_for();

            // TODO: Depending on the excution type, either copy to disk or compare with reference image
            save_image_to_disk(render_test, *image_readback_buffer, TEST_WIDTH, TEST_HEIGHT, TEST_IMAGE_FORMAT);

            render_test->cleanup_test();
        }
    }

    void execute_test(
        RenderTest* render_test,
        RenderGraphBuilder& builder,
        const RenderTestExecutionEnvironment& environment) const
    {
        MIZU_LOG_INFO(
            "Running Render Test '{}' on {}",
            render_test->get_test_name(),
            graphics_api_to_string(m_info.environment.graphics_api));

        render_test->prepare_test(environment);
        render_test->run_test(builder, environment);
    }

    void add_texture_readback_pass(
        RenderGraphBuilder& builder,
        RenderGraphResource output_texture,
        RenderGraphResource readback_buffer) const
    {
        struct PassData
        {
            RenderGraphResource output_texture;
            RenderGraphResource readback_buffer;
        };

        builder.add_pass<PassData>(
            "ReadbackOutputTexture",
            [&](RenderGraphPassBuilder& pass, PassData& data) {
                pass.set_hint(RenderGraphPassHint::Transfer);

                data.output_texture = pass.copy_src(output_texture);
                data.readback_buffer = pass.copy_dst(readback_buffer);
            },
            [](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
                const auto image = resources.get_image(data.output_texture);
                const auto buffer = resources.get_buffer(data.readback_buffer);

                command.copy_image_to_buffer(*image, *buffer);
            });
    }

    void save_image_to_disk(
        const RenderTest* render_test,
        const BufferResource& readback_buffer,
        uint32_t width,
        uint32_t height,
        ImageFormat format) const
    {
        const uint32_t format_size = get_image_format_size(format);
        const uint32_t bytes_per_row = width * format_size;

        const uint32_t row_pitch = ((bytes_per_row + 255) / 256) * 256;
        const uint32_t components = get_num_components(format);

        const uint8_t* mapped_data = readback_buffer.get_mapped_data();

        if (!mapped_data)
        {
            MIZU_LOG_ERROR("Failed to map readback buffer for image saving.");
            return;
        }

        const std::string filename = std::format(
            "{}_{}.png", render_test->get_test_name(), graphics_api_to_string(m_info.environment.graphics_api));

        const int32_t w = static_cast<int32_t>(width);
        const int32_t h = static_cast<int32_t>(height);
        const int32_t c = static_cast<int32_t>(components);
        const int32_t stride = static_cast<int32_t>(row_pitch);

        // Set compression level to 0
        extern int stbi_write_png_compression_level;
        stbi_write_png_compression_level = 0;

        stbi_write_png(filename.c_str(), w, h, c, static_cast<const void*>(mapped_data), stride);
    }

  private:
    RenderTestsInfo m_info{};
};

int main()
{
    std::vector<RenderTest*> render_tests{};
    render_tests.resize(MAX_TOTAL_RENDER_TESTS);

    RenderTestsRegistry& registry = RenderTestsRegistry::get();
    const std::span<RenderTest*> registered_render_tests = registry.get_render_tests();

    inplace_vector<RenderTestsInfo, 10> render_tests_info = {
#if MIZU_RENDER_CORE_VULKAN_ENABLED
        RenderTestsInfo{
            .environment = {.graphics_api = GraphicsApi::Vulkan},
        },
#endif
#if MIZU_RENDER_CORE_DX12_ENABLED
        RenderTestsInfo{
            .environment = {.graphics_api = GraphicsApi::Dx12},
        },
#endif
    };

    uint32_t test_num = 0;
    for (RenderTestsInfo& test_info : render_tests_info)
    {
        const uint32_t tests_offset = test_num;

        for (RenderTest* test : registered_render_tests)
        {
            if (test->should_run_test(test_info.environment))
            {
                render_tests[test_num++] = test;
            }
        }

        test_info.render_tests = std::span<RenderTest*>(render_tests.data() + tests_offset, test_num - tests_offset);
    }

    if (test_num == 0)
    {
        MIZU_LOG_INFO("No tests to run, exiting");
        return 0;
    }

    for (const RenderTestsInfo& tests_info : render_tests_info)
    {
        RenderTestsRunner runner{tests_info};
        runner.run_tests();
    }

    return 0;
}