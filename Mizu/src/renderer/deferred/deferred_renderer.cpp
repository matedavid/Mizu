#include "deferred_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/deferred/deferred_renderer_shaders.h"

#include "render_core/resources/camera.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/synchronization.h"

#include "scene/scene.h"

namespace Mizu
{

struct CameraInfoUBO
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 camera_position;
};

struct FrameInfo
{
    RGUniformBufferRef camera_info;
    RGStorageBufferRef point_lights;
    RGStorageBufferRef directional_lights;
    RGTextureRef result_texture;
};

struct DepthPrepassInfo
{
    RGTextureRef depth_prepass_texture;
};

struct ShadowmappingInfo
{
    RGTextureRef shadowmapping_texture;
    RGStorageBufferRef light_space_matrices;
};

struct GBufferInfo
{
    RGTextureRef albedo;
    RGTextureRef position;
    RGTextureRef normal;
    RGTextureRef metallic_roughness_ao;
};

struct ModelInfoData
{
    glm::mat4 model;
};

static std::shared_ptr<VertexBuffer> s_fullscreen_quad = nullptr;

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

    m_camera_ubo = UniformBuffer::create<CameraInfoUBO>(Renderer::get_allocator(), "CameraInfo");

    if (s_fullscreen_quad == nullptr)
    {
        struct FullscreenQuadVertex
        {
            glm::vec3 position;
            glm::vec2 texCoord;
        };

        if (Renderer::get_config().graphics_api == GraphicsAPI::Vulkan)
        {
            const std::vector<FullscreenQuadVertex> vertices_vulkan = {
                {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
                {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
                {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

                {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
                {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
                {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
            };
            s_fullscreen_quad = VertexBuffer::create(vertices_vulkan, Renderer::get_allocator());
        }
        else if (Renderer::get_config().graphics_api == GraphicsAPI::OpenGL)
        {
            const std::vector<FullscreenQuadVertex> vertices_opengl = {
                {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
                {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
                {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},

                {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
                {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
                {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            };
            s_fullscreen_quad = VertexBuffer::create(vertices_opengl, Renderer::get_allocator());
        }

        MIZU_ASSERT(s_fullscreen_quad != nullptr, "");
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

        std::vector<unsigned int> indices = {
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
    s_fullscreen_quad.reset();
    s_skybox_vertex_buffer.reset();
    s_skybox_index_buffer.reset();
}

void DeferredRenderer::render(const Camera& camera, const Texture2D& output)
{
    m_fence->wait_for();

    CameraInfoUBO camera_info_ubo{};
    camera_info_ubo.view = camera.view_matrix();
    camera_info_ubo.projection = camera.projection_matrix();
    camera_info_ubo.camera_position = camera.get_position();

    m_camera_ubo->update(camera_info_ubo);

    get_renderable_meshes();
    get_lights();

    //
    // Create RenderGraph
    //

    RenderGraphBuilder builder;
    RenderGraphBlackboard blackboard;

    const RGUniformBufferRef camera_ubo_ref = builder.register_external_buffer(*m_camera_ubo);
    const RGTextureRef result_texture_ref = builder.register_external_texture(output);

    const RGStorageBufferRef point_lights_ssbo_ref = builder.create_storage_buffer(m_point_lights, "PointLightsBuffer");
    const RGStorageBufferRef directional_lights_ssbo_ref = builder.create_storage_buffer(m_directional_lights, "DirectionalLightsBuffer");

    FrameInfo& frame_info = blackboard.add<FrameInfo>();
    frame_info.camera_info = camera_ubo_ref;
    frame_info.point_lights = point_lights_ssbo_ref;
    frame_info.directional_lights = directional_lights_ssbo_ref;
    frame_info.result_texture = result_texture_ref;

    add_depth_prepass(builder, blackboard);
    add_shadowmap_pass(builder, blackboard);
    add_gbuffer_pass(builder, blackboard);
    add_lighting_pass(builder, blackboard);

    if (m_config.skybox != nullptr)
    {
        add_skybox_pass(builder, blackboard);
    }

    //
    // Compile & Execute
    //

    const std::optional<RenderGraph> graph = builder.compile(m_command_buffer, *m_rg_allocator);
    MIZU_ASSERT(graph.has_value(), "Could not compile RenderGraph");

    m_graph = *graph;

    CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = m_fence;
    submit_info.signal_semaphore = m_render_semaphore;

    m_graph.execute(submit_info);
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

void DeferredRenderer::get_lights()
{
    m_point_lights.clear();
    m_directional_lights.clear();

    for (const Entity& light_entity : m_scene->view<PointLightComponent>())
    {
        const PointLightComponent& light = light_entity.get_component<PointLightComponent>();
        const TransformComponent& transform = light_entity.get_component<TransformComponent>();

        PointLight point_light{};
        point_light.position = glm::vec4(transform.position, 1.0f);
        point_light.color = glm::vec4(light.color, 1.0f);
        point_light.intensity = light.intensity;

        m_point_lights.push_back(point_light);
    }

    for (const Entity& light_entity : m_scene->view<DirectionalLightComponent>())
    {
        const DirectionalLightComponent& light = light_entity.get_component<DirectionalLightComponent>();
        const TransformComponent& transform = light_entity.get_component<TransformComponent>();

        glm::vec3 direction;
        direction.x = glm::cos(glm::radians(transform.rotation.x)) * glm::sin(glm::radians(transform.rotation.y));
        direction.y = glm::sin(glm::radians(transform.rotation.x));
        direction.z = glm::cos(glm::radians(transform.rotation.x)) * glm::cos(glm::radians(transform.rotation.y));
        direction = glm::normalize(direction);

        DirectionalLight directional_light{};
        directional_light.position = glm::vec4(transform.position, 1.0f);
        directional_light.direction = glm::vec4(direction, 0.0f);
        directional_light.color = glm::vec4(light.color, 1.0f);
        directional_light.intensity = light.intensity;
        directional_light.cast_shadows = light.cast_shadows;

        m_directional_lights.push_back(directional_light);
    }
}

void DeferredRenderer::add_depth_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const RGTextureRef depth_prepass_ref = builder.create_texture<Texture2D>(
        m_dimensions, ImageFormat::D32_SFLOAT, SamplingOptions{}, "DepthPrepassTexture");

    RGGraphicsPipelineDescription pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = true,
    };

    const RGFramebufferRef framebuffer_ref = builder.create_framebuffer(m_dimensions, {depth_prepass_ref});

    Deferred_DepthPrePass::Parameters params{};
    params.cameraInfo = blackboard.get<FrameInfo>().camera_info;

    Deferred_DepthPrePass depth_prepass_shader{};

    builder.add_pass(
        "DepthPrePass", depth_prepass_shader, params, pipeline, framebuffer_ref, [&](RenderCommandBuffer& command) {
            for (const RenderableMeshInfo& info : m_renderable_meshes_info)
            {
                ModelInfoData model_info{};
                model_info.model = info.transform;
                command.push_constant("modelInfo", model_info);

                RHIHelpers::draw_mesh(command, *info.mesh);
            }
        });

    blackboard.add<DepthPrepassInfo>().depth_prepass_texture = depth_prepass_ref;
}

void DeferredRenderer::add_shadowmap_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    // TODO: Shadows: https://hypesio.fr/en/dynamic-shadows-real-time-techs/

    static constexpr uint32_t SHADOWMAP_RESOLUTION = 1024;

    std::vector<glm::mat4> light_space_matrices;
    for (const DirectionalLight& light : m_directional_lights)
    {
        if (light.cast_shadows)
        {
            static constexpr float SIZE = 15.0f;

            const glm::mat4 light_view = glm::lookAt(
                glm::vec3(light.position), glm::vec3(light.position + light.direction), glm::vec3(0.0f, 1.0f, 0.0));
            const glm::mat4 light_proj = glm::orthoZO(-SIZE, SIZE, SIZE, -SIZE, 0.001f, 40.0f);

            const glm::mat4 light_space_matrix = light_proj * light_view;
            light_space_matrices.push_back(light_space_matrix);
        }
    }

    const uint32_t width = glm::max(SHADOWMAP_RESOLUTION * static_cast<uint32_t>(light_space_matrices.size()), 1u);
    const uint32_t height = !light_space_matrices.empty() ? SHADOWMAP_RESOLUTION : 1u;

    const RGTextureRef directional_shadowmaps_texture = builder.create_texture<Texture2D>(
        {width, height}, ImageFormat::D32_SFLOAT, SamplingOptions{}, "DirectionalShadowmapsTexture");

    const RGStorageBufferRef light_space_matrices_ssbo_ref = builder.create_storage_buffer(light_space_matrices, "LightSpaceMatricesBuffer");

    ShadowmappingInfo& shadowmapping_info = blackboard.add<ShadowmappingInfo>();
    shadowmapping_info.shadowmapping_texture = directional_shadowmaps_texture;
    shadowmapping_info.light_space_matrices = light_space_matrices_ssbo_ref;

    const RGFramebufferRef framebuffer_ref =
        builder.create_framebuffer({width, height}, {directional_shadowmaps_texture});

    RGGraphicsPipelineDescription pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = true,
    };

    Deferred_Shadowmapping::Parameters params{};
    params.lightSpaceMatrices = light_space_matrices_ssbo_ref;

    Deferred_Shadowmapping shader{};

    const uint32_t num_shadow_maps = static_cast<uint32_t>(light_space_matrices.size());
    builder.add_pass(
        "ShadowRenderingPass", shader, params, pipeline, framebuffer_ref, [=, this](RenderCommandBuffer& command) {
            for (const RenderableMeshInfo& mesh : m_renderable_meshes_info)
            {
                ModelInfoData data{};
                data.model = mesh.transform;
                command.push_constant("modelInfo", data);

                command.draw_indexed_instanced(
                    *mesh.mesh->vertex_buffer(), *mesh.mesh->index_buffer(), num_shadow_maps);
            }
        });
}

void DeferredRenderer::add_gbuffer_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    GBufferInfo& gbuffer_info = blackboard.add<GBufferInfo>();
    gbuffer_info.albedo =
        builder.create_texture<Texture2D>(m_dimensions, ImageFormat::RGBA8_SRGB, SamplingOptions{}, "GBuffer_Albedo");
    gbuffer_info.position = builder.create_texture<Texture2D>(
        m_dimensions, ImageFormat::RGBA32_SFLOAT, SamplingOptions{}, "GBuffer_Position");
    gbuffer_info.normal = builder.create_texture<Texture2D>(
        m_dimensions, ImageFormat::RGBA32_SFLOAT, SamplingOptions{}, "GBuffer_Normal");
    gbuffer_info.metallic_roughness_ao = builder.create_texture<Texture2D>(
        m_dimensions, ImageFormat::RGBA32_SFLOAT, SamplingOptions{}, "GBuffer_MetallicRoughnessAO");

    GraphicsPipeline::Description pipeline{};
    pipeline.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = false,
        .depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual,
    };

    const RGFramebufferRef framebuffer_ref =
        builder.create_framebuffer(m_dimensions,
                                   {
                                       gbuffer_info.albedo,
                                       gbuffer_info.position,
                                       gbuffer_info.normal,
                                       gbuffer_info.metallic_roughness_ao,
                                       blackboard.get<DepthPrepassInfo>().depth_prepass_texture,
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

            ModelInfoData model_info{};
            model_info.model = info.transform;
            command.push_constant("modelInfo", model_info);

            RHIHelpers::draw_mesh(command, *info.mesh);
        }
    });
}

void DeferredRenderer::add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const GBufferInfo& gbuffer_info = blackboard.get<GBufferInfo>();
    const ShadowmappingInfo& shadowmapping_info = blackboard.get<ShadowmappingInfo>();

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

    params.directionalLightSpaceMatrices = shadowmapping_info.light_space_matrices;
    params.directionalShadowmaps = shadowmapping_info.shadowmapping_texture;

    params.albedo = gbuffer_info.albedo;
    params.position = gbuffer_info.position;
    params.normal = gbuffer_info.normal;
    params.metallicRoughnessAO = gbuffer_info.metallic_roughness_ao;

    Deferred_PBRLighting lighting_shader{};

    builder.add_pass(
        "LightingPass", lighting_shader, params, pipeline, framebuffer_ref, [&](RenderCommandBuffer& command) {
            command.draw(*s_fullscreen_quad);
        });
}

void DeferredRenderer::add_skybox_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const RGTextureRef& depth_texture = blackboard.get<DepthPrepassInfo>().depth_prepass_texture;

    const RGCubemapRef skybox_ref = builder.register_external_cubemap(*m_config.skybox);
    const RGFramebufferRef framebuffer_ref =
        builder.create_framebuffer(m_dimensions, {frame_info.result_texture, depth_texture});

    Deferred_Skybox::Parameters params{};
    params.cameraInfo = frame_info.camera_info;
    params.skybox = skybox_ref;

    Deferred_Skybox skybox_shader{};

    RGGraphicsPipelineDescription pipeline_desc{};
    pipeline_desc.rasterization = RasterizationState{
        .front_face = RasterizationState::FrontFace::ClockWise,
    };
    pipeline_desc.depth_stencil.depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual;

    builder.add_pass("Skybox", skybox_shader, params, pipeline_desc, framebuffer_ref, [](RenderCommandBuffer& command) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(1.0f));

        ModelInfoData model_info{};
        model_info.model = model;

        command.push_constant("modelInfo", model_info);
        command.draw_indexed(*s_skybox_vertex_buffer, *s_skybox_index_buffer);
    });
}

} // namespace Mizu
