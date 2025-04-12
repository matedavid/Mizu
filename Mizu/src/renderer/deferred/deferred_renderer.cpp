#include "deferred_renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <random>

#include "renderer/deferred/deferred_renderer_shaders.h"

#include "render_core/resources/camera.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/sampler_state.h"
#include "render_core/rhi/synchronization.h"

#include "scene/scene.h"

namespace Mizu
{

struct FrameInfo
{
    RGUniformBufferRef camera_info;
    RGStorageBufferRef point_lights;
    RGStorageBufferRef directional_lights;
    RGImageViewRef result_texture;
};

struct CameraInfo
{
    glm::mat4 projection;
    glm::mat4 view;
    float znear, zfar;
};

struct ShadowmappingInfo
{
    RGImageViewRef shadowmapping_texture;
    RGStorageBufferRef light_space_matrices;
    RGStorageBufferRef cascade_splits;
};

struct GBufferInfo
{
    RGImageViewRef albedo;
    RGImageViewRef normal;
    RGImageViewRef metallic_roughness_ao;
    RGImageViewRef depth;
};

struct SSAOInfo
{
    RGImageViewRef ssao_texture;
};

struct GPUCameraInfo
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 inverse_view;
    glm::mat4 inverse_projection;
    glm::vec3 camera_position;
};

struct GPUModelInfo
{
    glm::mat4 model;
};

static std::shared_ptr<VertexBuffer> s_fullscreen_triangle = nullptr;

static std::shared_ptr<VertexBuffer> s_skybox_vertex_buffer = nullptr;
static std::shared_ptr<IndexBuffer> s_skybox_index_buffer = nullptr;

DeferredRenderer::DeferredRenderer(std::shared_ptr<Scene> scene,
                                   DeferredRendererConfig config,
                                   uint32_t width,
                                   uint32_t height)
    : m_scene(std::move(scene))
    , m_config(std::move(config))
    , m_dimensions({width, height})
{
    m_command_buffer = RenderCommandBuffer::create();
    m_rg_allocator = RenderGraphDeviceMemoryAllocator::create();

    m_fence = Fence::create();
    m_render_semaphore = Semaphore::create();

    m_camera_ubo = UniformBuffer::create<GPUCameraInfo>(Renderer::get_allocator(), "CameraInfo");

    {
        Texture2D::Description white_texture_desc{};
        white_texture_desc.dimensions = {1, 1};
        white_texture_desc.format = ImageFormat::R32_SFLOAT;
        white_texture_desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
        white_texture_desc.name = "WhiteTexture";

        std::vector<float> data = {1.0f};
        m_white_texture =
            Texture2D::create(white_texture_desc, reinterpret_cast<uint8_t*>(data.data()), Renderer::get_allocator());
    }

    if (s_fullscreen_triangle == nullptr)
    {
        struct FullscreenTriangleVertex
        {
            glm::vec3 position;
            glm::vec2 texCoord;
        };

        if (Renderer::get_config().graphics_api == GraphicsAPI::Vulkan)
        {
            const std::vector<FullscreenTriangleVertex> vertices_vulkan = {{
                {{3.0f, -1.0f, 0.0f}, {2.0f, 0.0f}},
                {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
                {{-1.0f, 3.0f, 0.0f}, {0.0f, 2.0f}},
            }};
            s_fullscreen_triangle = VertexBuffer::create(vertices_vulkan, Renderer::get_allocator());
        }
        else if (Renderer::get_config().graphics_api == GraphicsAPI::OpenGL)
        {
            MIZU_UNREACHABLE("OpenGL vertices are incorrect");
            const std::vector<FullscreenTriangleVertex> vertices_opengl = {
                {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
                {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
                {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},

                {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
                {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
                {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            };
            s_fullscreen_triangle = VertexBuffer::create(vertices_opengl, Renderer::get_allocator());
        }

        MIZU_ASSERT(s_fullscreen_triangle != nullptr, "");
    }

    if (s_skybox_vertex_buffer == nullptr)
    {
        // clang-format off
        const std::vector<glm::vec3> vertices = {
            // Front face
            {-0.5f, -0.5f,  0.5f}, // 0
            { 0.5f, -0.5f,  0.5f}, // 1
            { 0.5f,  0.5f,  0.5f}, // 2
            {-0.5f,  0.5f,  0.5f}, // 3

            // Back face
            {-0.5f, -0.5f, -0.5f}, // 4
            { 0.5f, -0.5f, -0.5f}, // 5
            { 0.5f,  0.5f, -0.5f}, // 6
            {-0.5f,  0.5f, -0.5f}  // 7
        };

        std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2,   0, 2, 3,
            // Right face
            1, 5, 6,   1, 6, 2,
            // Back face
            5, 4, 7,   5, 7, 6,
            // Left face
            4, 0, 3,   4, 3, 7,
            // Top face
            3, 2, 6,   3, 6, 7,
            // Bottom face
            4, 5, 1,   4, 1, 0
        };
        // clang-format on

        s_skybox_vertex_buffer = VertexBuffer::create(vertices, Renderer::get_allocator());
        s_skybox_index_buffer = IndexBuffer::create(indices, Renderer::get_allocator());

        MIZU_ASSERT(s_skybox_vertex_buffer != nullptr, "");
        MIZU_ASSERT(s_skybox_index_buffer != nullptr, "");
    }
}

DeferredRenderer::~DeferredRenderer()
{
    Renderer::wait_idle();
    s_fullscreen_triangle.reset();
    s_skybox_vertex_buffer.reset();
    s_skybox_index_buffer.reset();
}

void DeferredRenderer::render(const Camera& camera, const Texture2D& output)
{
    m_fence->wait_for();

    GPUCameraInfo camera_info_ubo{};
    camera_info_ubo.view = camera.view_matrix();
    camera_info_ubo.projection = camera.projection_matrix();
    camera_info_ubo.inverse_view = glm::inverse(camera.view_matrix());
    camera_info_ubo.inverse_projection = glm::inverse(camera.projection_matrix());
    camera_info_ubo.camera_position = camera.get_position();

    m_camera_ubo->update(camera_info_ubo);

    get_renderable_meshes();
    get_lights(camera);

    //
    // Create RenderGraph
    //

    RenderGraphBuilder builder;
    RenderGraphBlackboard blackboard;

    CameraInfo& camera_info = blackboard.add<CameraInfo>();
    camera_info.projection = camera.projection_matrix();
    camera_info.view = camera.view_matrix();
    camera_info.znear = camera.get_znear();
    camera_info.zfar = camera.get_zfar();

    const RGUniformBufferRef camera_ubo_ref = builder.register_external_buffer(*m_camera_ubo);
    const RGTextureRef result_texture_ref = builder.register_external_texture(output);

    const RGStorageBufferRef point_lights_ssbo_ref = builder.create_storage_buffer(m_point_lights, "PointLightsBuffer");
    const RGStorageBufferRef directional_lights_ssbo_ref =
        builder.create_storage_buffer(m_directional_lights, "DirectionalLightsBuffer");

    FrameInfo& frame_info = blackboard.add<FrameInfo>();
    frame_info.camera_info = camera_ubo_ref;
    frame_info.point_lights = point_lights_ssbo_ref;
    frame_info.directional_lights = directional_lights_ssbo_ref;
    frame_info.result_texture = builder.create_image_view(result_texture_ref);

    add_shadowmap_pass(builder, blackboard);
    add_gbuffer_pass(builder, blackboard);
    add_ssao_pass(builder, blackboard);
    add_lighting_pass(builder, blackboard);

    if (m_config.skybox != nullptr)
    {
        add_skybox_pass(builder, blackboard);
    }

    //
    // Compile & Execute
    //

    const std::optional<RenderGraph> graph = builder.compile(*m_rg_allocator);
    MIZU_ASSERT(graph.has_value(), "Could not compile RenderGraph");

    m_graph = *graph;

    CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = m_fence;
    submit_info.signal_semaphore = m_render_semaphore;

    m_graph.execute(*m_command_buffer, submit_info);
}

void DeferredRenderer::resize(uint32_t width, uint32_t height)
{
    if (m_dimensions == glm::uvec2(width, height))
    {
        return;
    }

    m_dimensions = glm::uvec2(width, height);
}

void DeferredRenderer::change_config(const DeferredRendererConfig& config)
{
    m_config = config;
}

void DeferredRenderer::get_renderable_meshes()
{
    m_renderable_meshes_info.clear();

    for (const auto& entity : m_scene->view<MeshRendererComponent>())
    {
        const MeshRendererComponent& mesh_renderer = entity.get_component<MeshRendererComponent>();
        const TransformComponent& transform = entity.get_component<TransformComponent>();

        glm::mat4 model(1.0f);
        model = glm::translate(model, transform.position);
        model = glm::rotate(model, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, transform.scale);

        RenderableMeshInfo info{};
        info.mesh = mesh_renderer.mesh;
        info.material = mesh_renderer.material;
        info.transform = model;

        m_renderable_meshes_info.push_back(info);
    }

    // Sort renderable meshes based on:
    // 1. By material (to prevent changes of pipeline state)
    // TODO: 2. By mesh (to combine entities with same material and mesh, and only bind vertex/index buffer once OR
    // instance the meshes???)
    std::ranges::sort(m_renderable_meshes_info, [](const RenderableMeshInfo& left, const RenderableMeshInfo& right) {
        if (left.material->get_hash() != right.material->get_hash())
        {
            return left.material->get_hash() < right.material->get_hash();
        }

        return left.mesh < right.mesh;
    });
}

void DeferredRenderer::get_lights(const Camera& camera)
{
    m_point_lights.clear();
    m_directional_lights.clear();

    for (const Entity& light_entity : m_scene->view<PointLightComponent>())
    {
        const PointLightComponent& light = light_entity.get_component<PointLightComponent>();
        const TransformComponent& transform = light_entity.get_component<TransformComponent>();

        PointLight point_light{};
        point_light.position = camera.view_matrix() * glm::vec4(transform.position, 1.0f);
        point_light.color = glm::vec4(light.color, 1.0f);
        point_light.intensity = light.intensity;

        m_point_lights.push_back(point_light);
    }

    for (const Entity& light_entity : m_scene->view<DirectionalLightComponent>())
    {
        const DirectionalLightComponent& light = light_entity.get_component<DirectionalLightComponent>();
        const TransformComponent& transform = light_entity.get_component<TransformComponent>();

        glm::vec3 direction{};
        direction.x = glm::cos(glm::radians(transform.rotation.x)) * glm::sin(glm::radians(transform.rotation.y));
        direction.y = glm::sin(glm::radians(transform.rotation.x));
        direction.z = glm::cos(glm::radians(transform.rotation.x)) * glm::cos(glm::radians(transform.rotation.y));
        direction = glm::normalize(direction);

        DirectionalLight directional_light{};
        directional_light.position = camera.view_matrix() * glm::vec4(transform.position, 1.0f);
        // TODO: Keeping in world space because cascaded shadow mapping needs the position in world space.
        directional_light.direction = glm::vec4(direction, 0.0f);
        directional_light.color = glm::vec4(light.color, 1.0f);
        directional_light.intensity = light.intensity;
        directional_light.cast_shadows = light.cast_shadows;

        m_directional_lights.push_back(directional_light);
    }
}

void DeferredRenderer::add_shadowmap_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    // TODO: Shadows: https://hypesio.fr/en/dynamic-shadows-real-time-techs/

    static constexpr uint32_t SHADOWMAP_RESOLUTION = 2048;

    std::vector<DirectionalLight> shadow_casting_directional_lights;
    shadow_casting_directional_lights.reserve(m_directional_lights.size());

    for (const DirectionalLight& light : m_directional_lights)
    {
        if (light.cast_shadows)
        {
            shadow_casting_directional_lights.push_back(light);
        }
    }

    const uint32_t num_shadow_casting_directional_lights =
        static_cast<uint32_t>(shadow_casting_directional_lights.size());

    if (m_config.num_cascades == 0)
    {
        MIZU_LOG_WARNING("Trying to request 0 cascades, will use 1 instead");
    }
    const uint32_t num_cascades = glm::max(m_config.num_cascades, 1u);

    const uint32_t width = glm::max(SHADOWMAP_RESOLUTION * num_shadow_casting_directional_lights, 1u);
    const uint32_t height = num_shadow_casting_directional_lights > 0 ? SHADOWMAP_RESOLUTION * num_cascades : 1u;

    const CameraInfo& camera_info = blackboard.get<CameraInfo>();

    const float clip_range = camera_info.zfar - camera_info.znear;
    const float clip_ratio = static_cast<float>(camera_info.zfar) / static_cast<float>(camera_info.znear);

    const auto get_cascade_split = [&](uint32_t idx) -> float {
        const float p = static_cast<float>(idx + 1) / static_cast<float>(num_cascades);

        const float log = camera_info.znear * glm::pow(clip_ratio, p);
        const float uniform = camera_info.znear + clip_range * p;
        const float d = m_config.cascade_split_lambda * (log - uniform) + uniform;

        return (d - camera_info.znear) / clip_range;
    };

    std::vector<glm::mat4> light_space_matrices(num_cascades * num_shadow_casting_directional_lights);
    std::vector<float> cascade_splits(num_cascades);

    for (uint32_t cascade_idx = 0; cascade_idx < num_cascades; ++cascade_idx)
    {
        cascade_splits[cascade_idx] = get_cascade_split(cascade_idx);

        const float last_split_dist = cascade_idx == 0 ? 0.0f : cascade_splits[cascade_idx - 1];
        const float split_dist = cascade_splits[cascade_idx];

        glm::vec3 frustum_corners[8] = {
            glm::vec3(-1.0f, 1.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 0.0f),
            glm::vec3(1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, -1.0f, 1.0f),
            glm::vec3(-1.0f, -1.0f, 1.0f),
        };

        const glm::mat4 inverted = glm::inverse(camera_info.projection * camera_info.view);
        for (uint32_t i = 0; i < 8; ++i)
        {
            const glm::vec4 inverted_corner = inverted * glm::vec4(frustum_corners[i], 1.0f);
            frustum_corners[i] = inverted_corner / inverted_corner.w;
        }

        for (uint32_t i = 0; i < 4; ++i)
        {
            const glm::vec3 dist = frustum_corners[i + 4] - frustum_corners[i];
            frustum_corners[i + 4] = frustum_corners[i] + (dist * split_dist);
            frustum_corners[i] = frustum_corners[i] + (dist * last_split_dist);
        }

        glm::vec3 frustum_center{0.0f};
        for (uint32_t i = 0; i < 8; ++i)
        {
            frustum_center += frustum_corners[i];
        }
        frustum_center /= 8.0f;

        float radius = 0.0f;
        for (uint32_t i = 0; i < 8; ++i)
        {
            const float distance = glm::length(frustum_corners[i] - frustum_center);
            radius = glm::max(radius, distance);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        const float texels_per_unit = static_cast<float>(SHADOWMAP_RESOLUTION) / (radius * 2.0f);
        const glm::mat4 texel_scale_matrix =
            glm::scale(glm::mat4(1.0f), glm::vec3(texels_per_unit, texels_per_unit, texels_per_unit));

        const glm::vec3 max_extents = glm::vec3(radius, radius, radius * m_config.z_scale_factor);
        const glm::vec3 min_extents = -max_extents;

        for (uint32_t light_idx = 0; light_idx < num_shadow_casting_directional_lights; ++light_idx)
        {
            const DirectionalLight& light = shadow_casting_directional_lights[light_idx];

            const glm::vec3 light_dir = glm::normalize(light.direction);

            // Texel correction: https://alextardif.com/shadowmapping.html
            const glm::mat4 texel_look_at =
                texel_scale_matrix * glm::lookAt(glm::vec3(0.0f), -light_dir, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 texel_look_at_inv = glm::inverse(texel_look_at);

            glm::vec3 texel_corrected_center = texel_look_at * glm::vec4(frustum_center, 1.0f);
            texel_corrected_center.x = glm::floor(texel_corrected_center.x);
            texel_corrected_center.y = glm::floor(texel_corrected_center.y);
            texel_corrected_center = texel_look_at_inv * glm::vec4(texel_corrected_center, 1.0f);

            const glm::mat4 light_view_matrix = glm::lookAt(
                frustum_center - light_dir * -min_extents.z, texel_corrected_center, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 light_projection_matrix = glm::orthoZO(
                min_extents.x, max_extents.x, max_extents.y, min_extents.y, 0.0f, max_extents.z - min_extents.z);

            light_space_matrices[cascade_idx * num_shadow_casting_directional_lights + light_idx] =
                light_projection_matrix * light_view_matrix;
        }
    }

    for (uint32_t i = 0; i < cascade_splits.size(); ++i)
    {
        cascade_splits[i] = (camera_info.znear + cascade_splits[i] * clip_range) * -1.0f;
    }

    const RGTextureRef directional_shadowmaps_texture =
        builder.create_texture<Texture2D>({width, height}, ImageFormat::D32_SFLOAT, "DirectionalShadowmapsTexture");

    const RGStorageBufferRef light_space_matrices_ssbo_ref =
        builder.create_storage_buffer(light_space_matrices, "LightSpaceMatricesBuffer");
    const RGStorageBufferRef cascade_splits_ssbo_ref =
        builder.create_storage_buffer(cascade_splits, "LightCascadeSplits");

    ShadowmappingInfo& shadowmapping_info = blackboard.add<ShadowmappingInfo>();
    shadowmapping_info.shadowmapping_texture = builder.create_image_view(directional_shadowmaps_texture);
    shadowmapping_info.light_space_matrices = light_space_matrices_ssbo_ref;
    shadowmapping_info.cascade_splits = cascade_splits_ssbo_ref;

    const RGFramebufferRef framebuffer_ref =
        builder.create_framebuffer({width, height}, {shadowmapping_info.shadowmapping_texture});

    RGGraphicsPipelineDescription pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = true,
        .depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual,
    };

    Deferred_Shadowmapping::Parameters params{};
    params.lightSpaceMatrices = light_space_matrices_ssbo_ref;

    Deferred_Shadowmapping shader{};

    const std::string pass_name = "CascadedShadowRendering";
    builder.add_pass(pass_name, shader, params, pipeline, framebuffer_ref, [=, this](RenderCommandBuffer& command) {
        for (const RenderableMeshInfo& mesh : m_renderable_meshes_info)
        {
            struct ShadowMappingModelInfo
            {
                glm::mat4 model;
                uint32_t num_lights;
                uint32_t num_cascades;
            };

            ShadowMappingModelInfo data{};
            data.model = mesh.transform;
            data.num_lights = num_shadow_casting_directional_lights;
            data.num_cascades = num_cascades;
            command.push_constant("modelInfo", data);

            command.draw_indexed_instanced(*mesh.mesh->vertex_buffer(),
                                           *mesh.mesh->index_buffer(),
                                           num_cascades * num_shadow_casting_directional_lights);
        }
    });
}

void DeferredRenderer::add_gbuffer_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const RGTextureRef albedo =
        builder.create_texture<Texture2D>(m_dimensions, ImageFormat::RGBA8_SRGB, "GBuffer_Albedo");
    const RGTextureRef normal =
        builder.create_texture<Texture2D>(m_dimensions, ImageFormat::RGBA32_SFLOAT, "GBuffer_Normal");
    const RGTextureRef metallic_roughness_ao =
        builder.create_texture<Texture2D>(m_dimensions, ImageFormat::RGBA32_SFLOAT, "GBuffer_MetallicRoughnessAO");
    const RGTextureRef depth = builder.create_texture<Texture2D>(m_dimensions, ImageFormat::D32_SFLOAT, "DepthTexture");

    GBufferInfo& gbuffer_info = blackboard.add<GBufferInfo>();

    gbuffer_info.albedo = builder.create_image_view(albedo);
    gbuffer_info.normal = builder.create_image_view(normal);
    gbuffer_info.metallic_roughness_ao = builder.create_image_view(metallic_roughness_ao);
    gbuffer_info.depth = builder.create_image_view(depth);

    GraphicsPipeline::Description pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = true,
        .depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual,
    };

    const RGFramebufferRef framebuffer_ref = builder.create_framebuffer(m_dimensions,
                                                                        {
                                                                            gbuffer_info.albedo,
                                                                            gbuffer_info.normal,
                                                                            gbuffer_info.metallic_roughness_ao,
                                                                            gbuffer_info.depth,
                                                                        });

    Deferred_PBROpaque::Parameters params{};
    params.cameraInfo = blackboard.get<FrameInfo>().camera_info;

    builder.add_pass("GBufferPass", params, framebuffer_ref, [=, this](RenderCommandBuffer& command) {
        size_t prev_material_hash = 0;

        for (const RenderableMeshInfo& info : m_renderable_meshes_info)
        {
            if (prev_material_hash == 0 || prev_material_hash != info.material->get_hash())
            {
                RHIHelpers::set_material(command, *info.material, pipeline);
                prev_material_hash = info.material->get_hash();
            }

            GPUModelInfo model_info{};
            model_info.model = info.transform;
            command.push_constant("modelInfo", model_info);

            RHIHelpers::draw_mesh(command, *info.mesh);
        }
    });
}

void DeferredRenderer::add_ssao_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    if (!m_config.ssao_enabled)
    {
        const RGTextureRef ssao_texture_ref = builder.register_external_texture(*m_white_texture);
        const RGImageViewRef ssao_texture_view_ref = builder.create_image_view(ssao_texture_ref);

        SSAOInfo& ssao_info = blackboard.add<SSAOInfo>();
        ssao_info.ssao_texture = ssao_texture_view_ref;

        return;
    }

    constexpr float SSAO_BIAS = 0.025f;

    static std::default_random_engine engine;
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    constexpr uint32_t SSAO_KERNEL_SIZE = 64;

    std::vector<glm::vec3> ssao_kernel(SSAO_KERNEL_SIZE);
    for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        glm::vec3 sample = glm::vec3(dist(engine) * 2.0f - 1.0f, dist(engine) * 2.0f - 1.0f, dist(engine));
        sample = glm::normalize(sample);
        sample *= dist(engine);

        float scale = static_cast<float>(i) / static_cast<float>(SSAO_KERNEL_SIZE);
        scale = glm::mix(0.1f, 1.0f, scale * scale);

        ssao_kernel[i] = sample * scale;
    }

    constexpr uint32_t SSAO_NOISE_DIM = 16;

    std::vector<glm::vec4> ssao_noise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
    for (uint32_t i = 0; i < SSAO_NOISE_DIM * SSAO_NOISE_DIM; ++i)
    {
        ssao_noise[i] = glm::vec4(dist(engine) * 2.0f - 1.0f, dist(engine) * 2.0f - 1.0f, 0.0f, 0.0f);
    }

    const RGStorageBufferRef ssao_kernel_ssbo_ref = builder.create_storage_buffer(ssao_kernel, "SSAOKernelBuffer");
    const RGTextureRef ssao_noise_texture_ref = builder.create_texture<Texture2D>(
        {SSAO_NOISE_DIM, SSAO_NOISE_DIM}, ImageFormat::RGBA32_SFLOAT, ssao_noise, "SSAONoiseTexture");

    MIZU_RG_SCOPED_GPU_DEBUG_LABEL(builder, "SSAO");

    constexpr uint32_t SSAO_GROUP_SIZE = 16;
    const glm::uvec3 group_count =
        RHIHelpers::compute_group_count(glm::uvec3(m_dimensions, 1), {SSAO_GROUP_SIZE, SSAO_GROUP_SIZE, 1});

    RGImageViewRef ssao_output_texture_view = RGImageViewRef::invalid();

    //
    // Main pass
    //

    {
        const RGTextureRef ssao_texture_ref =
            builder.create_texture<Texture2D>(m_dimensions, ImageFormat::R32_SFLOAT, "SSAOTexture");
        const RGImageViewRef ssao_texture_view_ref = builder.create_image_view(ssao_texture_ref);

        const FrameInfo& frame_info = blackboard.get<FrameInfo>();
        const GBufferInfo& gbuffer_info = blackboard.get<GBufferInfo>();

        Deferred_SSAOMain::Parameters ssao_main_params{};
        ssao_main_params.cameraInfo = frame_info.camera_info;
        ssao_main_params.ssaoKernel = ssao_kernel_ssbo_ref;
        ssao_main_params.ssaoNoise = builder.create_image_view(ssao_noise_texture_ref);
        ssao_main_params.gDepth = gbuffer_info.depth;
        ssao_main_params.gNormal = gbuffer_info.normal;
        ssao_main_params.sampler = RHIHelpers::get_sampler_state(SamplingOptions{
            .address_mode_u = ImageAddressMode::ClampToEdge,
            .address_mode_v = ImageAddressMode::ClampToEdge,
            .address_mode_w = ImageAddressMode::ClampToEdge,
        });
        ssao_main_params.mainOutput = ssao_texture_view_ref;

        Deferred_SSAOMain ssao_main_shader{};

        builder.add_pass("SSAOMain", ssao_main_shader, ssao_main_params, [=, this](RenderCommandBuffer& command) {
            struct SSAOMainInfo
            {
                uint32_t width, height;
                float radius;
                float bias;
            };

            SSAOMainInfo ssao_main_info{};
            ssao_main_info.width = m_dimensions.x;
            ssao_main_info.height = m_dimensions.y;
            ssao_main_info.radius = m_config.ssao_radius;
            ssao_main_info.bias = SSAO_BIAS;
            command.push_constant("ssaoMainInfo", ssao_main_info);

            command.dispatch(group_count);
        });

        ssao_output_texture_view = ssao_texture_view_ref;
    }

    //
    // Blur pass
    //

    if (m_config.ssao_blur_enabled)
    {
        const RGTextureRef ssao_blur_texture_ref =
            builder.create_texture<Texture2D>(m_dimensions, ImageFormat::R32_SFLOAT, "SSAOBlurTexture");
        const RGImageViewRef ssao_blur_texture_view_ref = builder.create_image_view(ssao_blur_texture_ref);

        Deferred_SSAOBlur::Parameters ssao_blur_params{};
        ssao_blur_params.blurInput = ssao_output_texture_view;
        ssao_blur_params.blurOutput = ssao_blur_texture_view_ref;
        ssao_blur_params.blurSampler = RHIHelpers::get_sampler_state(SamplingOptions{
            .address_mode_u = ImageAddressMode::ClampToEdge,
            .address_mode_v = ImageAddressMode::ClampToEdge,
            .address_mode_w = ImageAddressMode::ClampToEdge,
        });

        Deferred_SSAOBlur ssao_blur_shader{};

        builder.add_pass("SSAOBlur", ssao_blur_shader, ssao_blur_params, [=, this](RenderCommandBuffer& command) {
            struct SSAOBlurInfo
            {
                uint32_t width, height;
                uint32_t range;
            };

            constexpr uint32_t SSAO_BLUR_RANGE = 4;

            SSAOBlurInfo ssao_blur_info{};
            ssao_blur_info.width = m_dimensions.x;
            ssao_blur_info.height = m_dimensions.y;
            ssao_blur_info.range = SSAO_BLUR_RANGE;
            command.push_constant("ssaoBlurInfo", ssao_blur_info);

            command.dispatch(group_count);
        });

        ssao_output_texture_view = ssao_blur_texture_view_ref;
    }

    MIZU_ASSERT(ssao_output_texture_view != RGImageViewRef::invalid(), "SSAO output texture view has an invalid value");

    SSAOInfo& ssao_info = blackboard.add<SSAOInfo>();
    ssao_info.ssao_texture = ssao_output_texture_view;
}

void DeferredRenderer::add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const GBufferInfo& gbuffer_info = blackboard.get<GBufferInfo>();
    const ShadowmappingInfo& shadowmapping_info = blackboard.get<ShadowmappingInfo>();
    const SSAOInfo& ssao_info = blackboard.get<SSAOInfo>();

    RGGraphicsPipelineDescription pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = false,
        .depth_write = false,
    };

    const RGFramebufferRef framebuffer_ref = builder.create_framebuffer(m_dimensions, {frame_info.result_texture});

    Deferred_PBRLighting::Parameters params{};
    params.cameraInfo = frame_info.camera_info;
    params.pointLights = frame_info.point_lights;
    params.directionalLights = frame_info.directional_lights;

    params.directionalShadowmaps = shadowmapping_info.shadowmapping_texture;
    params.directionalLightSpaceMatrices = shadowmapping_info.light_space_matrices;
    params.directionalCascadeSplits = shadowmapping_info.cascade_splits;
    params.ssaoTexture = ssao_info.ssao_texture;

    params.albedo = gbuffer_info.albedo;
    params.normal = gbuffer_info.normal;
    params.metallicRoughnessAO = gbuffer_info.metallic_roughness_ao;
    params.depth = gbuffer_info.depth;
    params.sampler = RHIHelpers::get_sampler_state(SamplingOptions{
        .address_mode_u = ImageAddressMode::ClampToEdge,
        .address_mode_v = ImageAddressMode::ClampToEdge,
        .address_mode_w = ImageAddressMode::ClampToEdge,
    });

    Deferred_PBRLighting lighting_shader{};

    builder.add_pass(
        "LightingPass", lighting_shader, params, pipeline, framebuffer_ref, [&](RenderCommandBuffer& command) {
            command.draw(*s_fullscreen_triangle);
        });
}

void DeferredRenderer::add_skybox_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const RGImageViewRef& depth = blackboard.get<GBufferInfo>().depth;

    const RGCubemapRef skybox_ref = builder.register_external_cubemap(*m_config.skybox);
    const RGFramebufferRef framebuffer_ref =
        builder.create_framebuffer(m_dimensions, {frame_info.result_texture, depth});

    Deferred_Skybox::Parameters params{};
    params.cameraInfo = frame_info.camera_info;
    params.skybox = builder.create_image_view(skybox_ref, ImageResourceViewRange::from_layers(0, 6));
    params.sampler = RHIHelpers::get_sampler_state(SamplingOptions{});

    Deferred_Skybox skybox_shader{};

    RGGraphicsPipelineDescription pipeline_desc{};
    pipeline_desc.rasterization = RasterizationState{
        .front_face = RasterizationState::FrontFace::ClockWise,
    };
    pipeline_desc.depth_stencil.depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual;

    builder.add_pass("Skybox", skybox_shader, params, pipeline_desc, framebuffer_ref, [](RenderCommandBuffer& command) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(1.0f));

        GPUModelInfo model_info{};
        model_info.model = model;

        command.push_constant("modelInfo", model_info);
        command.draw_indexed(*s_skybox_vertex_buffer, *s_skybox_index_buffer);
    });
}

} // namespace Mizu
