#include <cstdint>
#include <filesystem>
#include <format>
#include <glm/gtc/constants.hpp>
#include <nlohmann/json.hpp>
#include <span>
#include <stb_image.h>
#include <stb_image_write.h>
#include <vector>

#include "base/io/filesystem.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render/systems/shader_manager.h"
#include "render_core/rhi/device.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/synchronization.h"

#include "render_tests.pipeline/render_test_shaders.h"
#include "runner/render_tests_runner.h"

using namespace Mizu;

RenderTestsRunner::RenderTestsRunner(RenderTestsInfo info) : m_info(std::move(info))
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

    if (!std::filesystem::exists(m_info.reference_images_path))
    {
        std::filesystem::create_directories(m_info.reference_images_path);
    }
}

RenderTestsRunner::~RenderTestsRunner()
{
    ShaderManager::get().reset();
    PipelineCache::get().reset();
    SamplerStateCache::get().reset();

    delete g_render_device;
    Device::free();
}

void RenderTestsRunner::run_tests()
{
    constexpr uint64_t FRAME_ALLOCATOR_SIZE = 64ull * 1024 * 1024; // 64 MB
    FrameLinearAllocator frame_allocator{1, FRAME_ALLOCATOR_SIZE, "RenderTest_FrameAllocator"};

    BufferDescription readback_buffer_desc{};
    readback_buffer_desc.size = TEST_WIDTH * TEST_HEIGHT * get_image_format_size(TEST_IMAGE_FORMAT);
    readback_buffer_desc.stride = sizeof(uint8_t);
    readback_buffer_desc.usage = BufferUsageBits::TransferDst | BufferUsageBits::HostVisible;
    readback_buffer_desc.name = "RenderTest_ReadbackBuffer";
    const auto image_readback_buffer = g_render_device->create_buffer(readback_buffer_desc);

    BufferDescription comparison_result_readback_buffer_desc{};
    comparison_result_readback_buffer_desc.size = TEST_WIDTH * TEST_HEIGHT * sizeof(float);
    comparison_result_readback_buffer_desc.stride = sizeof(float);
    comparison_result_readback_buffer_desc.usage = BufferUsageBits::HostVisible | BufferUsageBits::TransferDst;
    comparison_result_readback_buffer_desc.name = "RenderTest_ResultBuffer";
    const auto comparison_result_readback_buffer =
        g_render_device->create_buffer(comparison_result_readback_buffer_desc);

    const auto fence = g_render_device->create_fence(false);

    RenderGraphResourceRegistry resource_registry{false};
    const auto transient_memory_pool = g_render_device->create_transient_memory_pool("RenderTest_TransientMemoryPool");

    RenderGraph render_graph{};
    RenderGraphBuilder builder{};

    for (RenderTest* render_test : m_info.render_tests)
    {
        render_graph.reset();
        builder.reset();

        if (m_info.execution_type == ExecutionType::CompareImages
            && !std::filesystem::exists(get_reference_image_path(*render_test)))
        {
            MIZU_LOG_ERROR(
                "Reference image for test '{}' not found at path: {}, Skipping test",
                render_test->get_test_name(),
                get_reference_image_path(*render_test).string());

            m_results.failed_tests += 1;

            continue;
        }

        ImageDescription image_desc{};
        image_desc.width = TEST_WIDTH;
        image_desc.height = TEST_HEIGHT;
        image_desc.usage = ImageUsageBits::Attachment | ImageUsageBits::Sampled | ImageUsageBits::TransferSrc;
        image_desc.flags = ImageFlagBits::MutableFormat;
        image_desc.format = TEST_IMAGE_FORMAT;
        image_desc.name = "RenderTest_OutputImage";

        const RenderGraphResource output_texture = builder.create_texture(image_desc);

        const RenderTestExecutionEnvironment execution_environment{
            .graphics_api = m_info.environment.graphics_api,
            .frame_allocator = &frame_allocator,
            .output_width = TEST_WIDTH,
            .output_height = TEST_HEIGHT,
            .output_texture = output_texture,
        };

        add_test_execution_pass(builder, render_test, execution_environment);

        const RenderGraphResource readback_buffer = builder.register_external_buffer(
            image_readback_buffer, {BufferResourceState::TransferDst, BufferResourceState::TransferDst});

        if (m_info.execution_type == ExecutionType::UpdateReferenceImages)
        {
            add_texture_readback_pass(builder, output_texture, readback_buffer);
        }
        else if (m_info.execution_type == ExecutionType::CompareImages)
        {
            const RenderGraphResource reference_texture =
                add_reference_texture_upload_pass(builder, frame_allocator, *render_test);

            add_texture_compare_pass(builder, output_texture, reference_texture, comparison_result_readback_buffer);
            add_texture_readback_pass(builder, output_texture, readback_buffer);
        }

        const RenderGraphBuilderCompileOptions compile_options{
            *transient_memory_pool,
            resource_registry,
        };

        builder.compile(render_graph, compile_options);

        render_graph.execute({
            .signal_fence = fence,
        });

        fence->wait_for();

        if (m_info.execution_type == ExecutionType::UpdateReferenceImages)
        {
            save_updated_reference_image(*render_test, *image_readback_buffer);
        }
        else if (m_info.execution_type == ExecutionType::CompareImages)
        {
            const bool success =
                save_compare_images_result(*render_test, *image_readback_buffer, *comparison_result_readback_buffer);

            if (success)
            {
                m_results.passed_tests += 1;
            }
            else
            {
                m_results.failed_tests += 1;
            }
        }

        render_test->cleanup_test();
    }
}

void RenderTestsRunner::add_test_execution_pass(
    RenderGraphBuilder& builder,
    RenderTest* render_test,
    const RenderTestExecutionEnvironment& environment) const
{
    MIZU_LOG_INFO(
        "Running Render Test '{}' on {}",
        get_full_test_name(*render_test),
        graphics_api_to_string(m_info.environment.graphics_api));

    render_test->prepare_test(environment);
    render_test->run_test(builder, environment);
}

void RenderTestsRunner::add_texture_readback_pass(
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

RenderGraphResource RenderTestsRunner::add_reference_texture_upload_pass(
    RenderGraphBuilder& builder,
    FrameLinearAllocator& frame_allocator,
    const RenderTest& render_test) const
{
    const std::filesystem::path reference_image_path = get_reference_image_path(render_test);
    if (!std::filesystem::exists(reference_image_path))
    {
        MIZU_ASSERT(
            false,
            "No reference image found for test '{}', expected at path: {}",
            get_full_test_name(render_test),
            reference_image_path.string());
        return RenderGraphResource{};
    }

    int32_t w = 0, h = 0, c = 0;

    const std::string reference_image_path_str = reference_image_path.string();
    stbi_uc* data = stbi_load(reference_image_path_str.c_str(), &w, &h, &c, 4);

    if (!data)
    {
        MIZU_LOG_ERROR("Failed to load reference image: {}", reference_image_path.string());
        return RenderGraphResource{};
    }

    const uint32_t width = static_cast<uint32_t>(w);
    const uint32_t height = static_cast<uint32_t>(h);

    const uint64_t reference_image_size = width * height * 4ull;

    const FrameAllocation& allocation = frame_allocator.allocate(reference_image_size, 256, 1);
    allocation.upload(std::span(data, reference_image_size));

    stbi_image_free(data);

    ImageDescription reference_image_desc{};
    reference_image_desc.width = width;
    reference_image_desc.height = height;
    reference_image_desc.format = ImageFormat::R8G8B8A8_UNORM;
    reference_image_desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    reference_image_desc.flags = ImageFlagBits::MutableFormat;
    reference_image_desc.name = "RenderTest_ReferenceImage";
    const RenderGraphResource reference_texture = builder.create_texture(reference_image_desc);

    struct PassData
    {
        RenderGraphResource reference_texture;
    };

    builder.add_pass<PassData>(
        "UploadReferenceTexture",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Transfer);

            data.reference_texture = pass.copy_dst(reference_texture);
        },
        [=](CommandBuffer& command, const PassData& pass_data, const RenderGraphPassResources& resources) {
            const BufferResource& staging_buffer = *allocation.view.buffer;
            const ImageResource& reference_image = *resources.get_image(pass_data.reference_texture);

            const CopyBufferToImageInfo copy_info{
                .buffer_offset = allocation.view.desc.offset,
                .image_extent = {width, height, 1},
            };

            command.copy_buffer_to_image(staging_buffer, reference_image, copy_info);
        });

    return reference_texture;
}

void RenderTestsRunner::add_texture_compare_pass(
    RenderGraphBuilder& builder,
    RenderGraphResource pending_texture,
    RenderGraphResource reference_texture,
    std::shared_ptr<BufferResource> comparison_result_readback_buffer) const
{
    BufferDescription comparison_result_buffer_desc{};
    comparison_result_buffer_desc.size = comparison_result_readback_buffer->get_size();
    comparison_result_buffer_desc.stride = comparison_result_readback_buffer->get_stride();
    comparison_result_buffer_desc.usage = BufferUsageBits::UnorderedAccess | BufferUsageBits::TransferSrc;
    comparison_result_buffer_desc.name = "RenderTest_ComparisonResultBuffer";
    const RenderGraphResource comparison_result_buffer = builder.create_buffer(comparison_result_buffer_desc);

    const RenderGraphResource comparison_result_readback_buffer_ref = builder.register_external_buffer(
        comparison_result_readback_buffer, {BufferResourceState::Undefined, BufferResourceState::ShaderReadOnly});

    struct PassData
    {
        RenderGraphResource pending_texture;
        RenderGraphResource reference_texture;
        RenderGraphResource comparison_result_buffer;
        RenderGraphResource comparison_result_readback_buffer;
    };

    builder.add_pass<PassData>(
        "ComparePass",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.pending_texture = pass.read(pending_texture);
            data.reference_texture = pass.read(reference_texture);
            data.comparison_result_buffer = pass.write(comparison_result_buffer);
            data.comparison_result_readback_buffer = pass.copy_dst(comparison_result_readback_buffer_ref);
        },
        [=](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            const auto pipeline = get_compute_pipeline(CompareImagesShaderCs{});
            command.bind_pipeline(pipeline);

            const auto pending_texture = resources.get_image(data.pending_texture);
            const auto reference_texture = resources.get_image(data.reference_texture);
            const auto comparison_result_buffer = resources.get_buffer(data.comparison_result_buffer);
            const auto comparison_result_readback_buffer = resources.get_buffer(data.comparison_result_readback_buffer);

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(ComparePassLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, 1, ShaderType::Compute)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(1, 1, ShaderType::Compute)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(0, 1, ShaderType::Compute)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            std::array writes = {
                WriteDescriptor::TextureSrv(0, ImageResourceView::create(pending_texture)),
                WriteDescriptor::TextureSrv(1, ImageResourceView::create(reference_texture)),
                WriteDescriptor::StructuredBufferUav(0, BufferResourceView::create(comparison_result_buffer)),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                ComparePassLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.bind_descriptor_set(descriptor_set, 0);

            const glm::uvec3 group_count = compute_group_count({TEST_WIDTH, TEST_HEIGHT, 1}, {8, 8, 1});
            command.dispatch(group_count);

            command.transition_resource(
                *comparison_result_buffer, BufferResourceState::UnorderedAccess, BufferResourceState::TransferSrc);

            command.copy_buffer_to_buffer(*comparison_result_buffer, *comparison_result_readback_buffer);

            command.transition_resource(
                *comparison_result_buffer, BufferResourceState::TransferSrc, BufferResourceState::UnorderedAccess);
        });
}

void RenderTestsRunner::save_updated_reference_image(
    const RenderTest& render_test,
    const Mizu::BufferResource& image_readback_buffer) const
{
    const std::filesystem::path reference_image_path = get_reference_image_path(render_test);

    if (!std::filesystem::exists(reference_image_path))
    {
        std::filesystem::create_directories(reference_image_path.parent_path());
    }

    const uint32_t format_size = get_image_format_size(TEST_IMAGE_FORMAT);
    const uint32_t bytes_per_row = TEST_WIDTH * format_size;
    const uint32_t row_pitch = ((bytes_per_row + 255) / 256) * 256;
    const uint32_t components = get_num_components(TEST_IMAGE_FORMAT);
    const uint8_t* data = image_readback_buffer.get_mapped_data();

    if (data)
    {
        save_image_to_disk(reference_image_path.string(), data, TEST_WIDTH, TEST_HEIGHT, row_pitch, components);
    }
}

bool RenderTestsRunner::save_compare_images_result(
    const RenderTest& render_test,
    const Mizu::BufferResource& image_readback_buffer,
    const Mizu::BufferResource& result_readback_buffer) const
{
    const size_t comparison_result_buffer_num = result_readback_buffer.get_size() / sizeof(float);
    const std::span<float> comparison_result_data = std::span{
        reinterpret_cast<float*>(result_readback_buffer.get_mapped_data()),
        comparison_result_buffer_num,
    };

    constexpr float ERROR_THRESHOLD = 0.01f;

    const auto epsilon_greater_equal = [](float a, float b) {
        return (a - b) >= -glm::epsilon<float>();
    };

    bool success = true;
    for (float value : comparison_result_data)
    {
        if (epsilon_greater_equal(value, ERROR_THRESHOLD))
        {
            success = false;
        }
    }

    if (!success)
    {
        MIZU_LOG_ERROR(
            "Render test '{}' failed on {}",
            get_full_test_name(render_test),
            graphics_api_to_string(m_info.environment.graphics_api));
    }

    const std::string full_test_name = get_full_test_name(render_test);
    const std::filesystem::path result_test_path =
        m_info.session_path / full_test_name / graphics_api_to_string(m_info.environment.graphics_api);

    if (!std::filesystem::exists(result_test_path))
    {
        std::filesystem::create_directories(result_test_path);
    }

    // Save pending image
    const std::filesystem::path pending_image_path = result_test_path / "Pending.bmp";
    {
        const uint32_t format_size = get_image_format_size(TEST_IMAGE_FORMAT);
        const uint32_t bytes_per_row = TEST_WIDTH * format_size;
        const uint32_t row_pitch = ((bytes_per_row + 255) / 256) * 256;
        const uint32_t components = get_num_components(TEST_IMAGE_FORMAT);
        const uint8_t* data = image_readback_buffer.get_mapped_data();

        if (data)
        {
            save_image_to_disk(pending_image_path.string(), data, TEST_WIDTH, TEST_HEIGHT, row_pitch, components);
        }
    }

    // Save reference image
    const std::filesystem::path reference_image_path = get_reference_image_path(render_test);
    std::filesystem::copy_file(
        reference_image_path, result_test_path / "Reference.bmp", std::filesystem::copy_options::overwrite_existing);

    // Save comparison result
    const std::filesystem::path comparison_image_path = result_test_path / "Comparison.bmp";

    const uint32_t num_pixels = TEST_WIDTH * TEST_HEIGHT;
    std::vector<uint8_t> comparison_image_data(num_pixels * 4, 0);

    for (uint32_t i = 0; i < num_pixels; ++i)
    {
        const float value = comparison_result_data[i];
        const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        comparison_image_data[i * 4 + 0] = static_cast<uint8_t>(clamped * 255.0f);
        comparison_image_data[i * 4 + 1] = 0;
        comparison_image_data[i * 4 + 2] = 0;
        comparison_image_data[i * 4 + 3] = 255;
    }

    save_image_to_disk(
        comparison_image_path.string(), comparison_image_data.data(), TEST_WIDTH, TEST_HEIGHT, TEST_WIDTH * 4, 4);

    nlohmann::json json_result;
    json_result["success"] = success;
    json_result["test_name"] = full_test_name;
    json_result["graphics_api"] = graphics_api_to_string(m_info.environment.graphics_api);

    Filesystem::write_file_string(result_test_path / "Results.json", json_result.dump(4));

    return success;
}

void RenderTestsRunner::save_image_to_disk(
    std::string_view filename,
    const uint8_t* data,
    uint32_t width,
    uint32_t height,
    uint32_t stride,
    uint32_t components) const
{
    if (!data)
    {
        MIZU_LOG_ERROR("Failed to map readback buffer for image saving.");
        return;
    }

    const int32_t w = static_cast<int32_t>(width);
    const int32_t h = static_cast<int32_t>(height);
    const int32_t c = static_cast<int32_t>(components);

    const uint32_t tight_row_size = width * components;

    std::vector<uint8_t> packed_data;
    const uint8_t* write_data = data;

    if (stride != tight_row_size)
    {
        packed_data.resize(static_cast<size_t>(tight_row_size) * height);
        for (uint32_t row = 0; row < height; ++row)
        {
            memcpy(packed_data.data() + row * tight_row_size, data + row * stride, tight_row_size);
        }

        write_data = packed_data.data();
    }

    stbi_write_bmp(filename.data(), w, h, c, static_cast<const void*>(write_data));
}

std::filesystem::path RenderTestsRunner::get_reference_image_path(const RenderTest& render_test) const
{
    const std::string full_test_name = get_full_test_name(render_test);
    const std::string_view graphics_api_str = graphics_api_to_string(m_info.environment.graphics_api);

    return m_info.reference_images_path / full_test_name / std::format("{}.bmp", graphics_api_str);
}

std::string RenderTestsRunner::get_full_test_name(const RenderTest& render_test) const
{
    return std::format("{}.{}", render_test.get_test_group_name(), render_test.get_test_name());
}
