#include "imgui/imgui_render_module.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <imgui.h>
#include <memory>
#include <optional>
#include <stack>
#include <unordered_map>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "base/math/math.h"
#include "core/runtime.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render_core/rhi/descriptors.h"

#include "imgui.pipeline/imgui_shaders.h"
#include "imgui/imgui_panels.h"

namespace Mizu
{

namespace
{

class TexturePool
{
    static constexpr uint32_t NUM_BINDLESS_TEXTURES = 100;

  public:
    TexturePool()
    {
        for (uint32_t i = 0; i < NUM_BINDLESS_TEXTURES; ++i)
        {
            m_available_bindless_slots.push(NUM_BINDLESS_TEXTURES - i - 1);
        }

        // clang-format off
        MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(ImGuiTextureBindlessLayout)
            MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, BINDLESS_DESCRIPTOR_COUNT, ShaderType::Fragment)
        MIZU_END_DESCRIPTOR_SET_LAYOUT()
        // clang-format on

        m_textures_descriptor_set = g_render_device->allocate_descriptor_set(
            ImGuiTextureBindlessLayout::get_layout(), DescriptorSetAllocationType::Bindless, NUM_BINDLESS_TEXTURES);
    }

    bool create(ImGuiTextureId id, const ImageDescription& desc)
    {
        const auto it = m_texture_entries_map.find(id);
        if (it != m_texture_entries_map.end())
        {
            MIZU_LOG_ERROR("Texture with id {} is already created", id);
            return false;
        }

        if (m_available_bindless_slots.empty())
        {
            MIZU_LOG_ERROR("Not enough bindless slots for ImGui texture with id: {}", id);
            return false;
        }

        const uint32_t bindless_slot = m_available_bindless_slots.top();
        m_available_bindless_slots.pop();

        const TextureEntry entry{
            .resource = g_render_device->create_image(desc),
            .bindless_slot = bindless_slot,
        };

        m_texture_entries_map.emplace(id, entry);

        const std::array writes = {
            WriteDescriptor::TextureSrv(0, ImageResourceView::create(entry.resource)),
        };
        m_textures_descriptor_set->update(writes, bindless_slot);

        return true;
    }

    void destroy(ImGuiTextureId id)
    {
        const auto it = m_texture_entries_map.find(id);
        if (it == m_texture_entries_map.end())
        {
            MIZU_LOG_ERROR("Texture with id {} does not exist", id);
            return;
        }

        m_pending_releases.push_back({
            .resource = std::move(it->second.resource),
            .bindless_slot = it->second.bindless_slot,
            .frames = 4, // TODO: DON'T USE A HARDCODED VALUE
        });

        m_texture_entries_map.erase(it);
    }

    void track_releases()
    {
        auto it = m_pending_releases.begin();

        while (it != m_pending_releases.end())
        {
            if (--it->frames == 0)
            {
                it->resource.reset();
                m_available_bindless_slots.push(it->bindless_slot);
                it = m_pending_releases.erase(it);
            }
            else
            {
                it = std::next(it);
            }
        }
    }

    uint32_t get_num_textures() const { return static_cast<uint32_t>(m_texture_entries_map.size()); }

    ImageResource* get_texture(ImGuiTextureId id) const
    {
        const auto it = m_texture_entries_map.find(id);
        if (it == m_texture_entries_map.end())
        {
            MIZU_LOG_ERROR("Texture with id {} does not exist", id);
            return nullptr;
        }

        return it->second.resource.get();
    }

    std::optional<uint32_t> get_bindless_slot(ImGuiTextureId id) const
    {
        const auto it = m_texture_entries_map.find(id);
        if (it == m_texture_entries_map.end())
        {
            MIZU_LOG_ERROR("Texture with id {} does not exist", id);
            return std::nullopt;
        }

        return it->second.bindless_slot;
    }

    std::shared_ptr<DescriptorSet> get_bindless_descriptor_set() const { return m_textures_descriptor_set; }

  private:
    struct TextureEntry
    {
        std::shared_ptr<ImageResource> resource;
        uint32_t bindless_slot;
    };

    std::unordered_map<ImGuiTextureId, TextureEntry> m_texture_entries_map{};

    std::stack<uint32_t> m_available_bindless_slots{};
    std::shared_ptr<DescriptorSet> m_textures_descriptor_set{};

    struct PendingRelease
    {
        std::shared_ptr<ImageResource> resource;
        uint32_t bindless_slot;
        uint32_t frames;
    };

    std::vector<PendingRelease> m_pending_releases{};
};

} // namespace

static TexturePool* s_texture_pool = nullptr;
static bool s_indirect_drawing = true;

bool ImGuiRenderModule::init()
{
    g_imgui_state_manager->register_rend_consumer(this);

    s_texture_pool = new TexturePool{};

    ImGuiPanels::register_panel(ImGuiPanelBuilder::window("ImGuiRenderModule").menu("ImGui").closed(), [&]() {
        ImGui::Checkbox("Indirect Drawing", &s_indirect_drawing);

        const uint32_t num_frame_data = static_cast<uint32_t>(m_frame_data_list.size());
        ImGui::Text("Num Frame Data: %u", num_frame_data);

        const uint32_t num_draw_commands = m_debug_num_draw_commands.load(std::memory_order_relaxed);
        ImGui::Text("Num Draw Commands: %u", num_draw_commands);

        const uint32_t num_textures = s_texture_pool->get_num_textures();
        ImGui::Text("Num Textures: %u", num_textures);
    });

    return true;
}

void ImGuiRenderModule::shutdown()
{
    delete s_texture_pool;
    s_texture_pool = nullptr;

    g_imgui_state_manager->unregister_rend_consumer(this);
}

JobHandle ImGuiRenderModule::create_update_jobs(const RenderModuleUpdateContext& ctx)
{
    return g_job_system->schedule([]() { s_texture_pool->track_releases(); }).depends_on(ctx.wait_job).submit();
}

void ImGuiRenderModule::build_render_graph(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard,
    const RenderModuleFrameData& frame_data)
{
    if (m_frame_data_list.empty() && m_pending_texture_ops.empty())
        return;

    MIZU_PROFILE_SCOPED;

    const RenderSystemsData& systems_data = blackboard.get<RenderSystemsData>();
    FrameLinearAllocator& frame_allocator = systems_data.frame_allocator;

    m_draw_commands.clear();

    // TODO: Temporary allocation, also, could precompute number of vertices and indices when preparing the
    // FrameSnapshot on ImGuiContext to reserve
    std::vector<ImGuiVertex> vertices{};
    std::vector<uint32_t> indices{};
    std::vector<ImGuiDrawData> draw_data{};

    for (const ImGuiFrameData& data : m_frame_data_list)
    {
        if (data.snapshot == nullptr || !data.snapshot->valid)
            continue;

        const glm::vec2 framebuffer_size = data.snapshot->display_size * data.snapshot->framebuffer_scale;

        // Engine NDC is Y up, ImGui coordinates are Y down
        const glm::vec2 scale = glm::vec2(2.0f, -2.0f) / data.snapshot->display_size;
        const glm::vec2 translate = glm::vec2(-1.0f, 1.0f) - data.snapshot->display_pos * scale;

        for (const ImGuiDrawList& cmd : data.snapshot->cmd_lists)
        {
            const uint32_t vertex_offset = static_cast<uint32_t>(vertices.size());
            const uint32_t index_offset = static_cast<uint32_t>(indices.size());

            std::copy(cmd.vertices.begin(), cmd.vertices.end(), std::back_inserter(vertices));
            std::copy(cmd.indices.begin(), cmd.indices.end(), std::back_inserter(indices));

            for (const ImGuiDrawCommand& draw_cmd : cmd.commands)
            {
                const glm::vec4& clip_rect = draw_cmd.clip_rect;
                const glm::vec2 clip_rect_min = glm::vec2(clip_rect.x, clip_rect.y);
                const glm::vec2 clip_rect_max = glm::vec2(clip_rect.z, clip_rect.w);

                const glm::vec2 framebuffer_clip_min =
                    (clip_rect_min - data.snapshot->display_pos) * data.snapshot->framebuffer_scale;
                const glm::vec2 framebuffer_clip_max =
                    (clip_rect_max - data.snapshot->display_pos) * data.snapshot->framebuffer_scale;

                const glm::vec2 clip_min = glm::max(framebuffer_clip_min, glm::vec2(0.0f));
                const glm::vec2 clip_max = glm::min(framebuffer_clip_max, framebuffer_size);

                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                {
                    continue;
                }

                const std::optional<uint32_t> bindless_slot = s_texture_pool->get_bindless_slot(draw_cmd.texture_id);
                if (!bindless_slot.has_value())
                {
                    continue;
                }

                const uint32_t draw_data_idx = static_cast<uint32_t>(draw_data.size());
                draw_data.push_back({
                    .scale = scale,
                    .translate = translate,
                    .clip_min = clip_rect_min,
                    .clip_max = clip_rect_max,
                    .vertex_offset = vertex_offset + draw_cmd.vertex_offset,
                    .index_offset = index_offset + draw_cmd.index_offset,
                    .bindless_slot = *bindless_slot,
                });

                m_draw_commands.push_back({
                    .draw_index = draw_data_idx,
                    .vertex_count = draw_cmd.element_count,
                    .instance_count = 1,
                    .first_vertex = 0,
                    .first_instance = 0,
                });
            }
        }
    }

    if (m_draw_commands.empty() && m_pending_texture_ops.empty())
    {
        return;
    }

    m_debug_num_draw_commands.store(static_cast<uint32_t>(m_draw_commands.size()), std::memory_order_relaxed);

    struct PassData
    {
        RenderGraphResource output;

        FrameAllocation vertex_alloc;
        FrameAllocation index_alloc;
        FrameAllocation draw_data_alloc;
        FrameAllocation indirect_alloc;

        bool indirect_drawing;
    };

    builder.add_pass<PassData>(
        "ImGui",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.indirect_drawing = s_indirect_drawing;

            data.output = pass.attachment_write(frame_data.output_texture);

            if (m_draw_commands.empty())
                return;

            data.vertex_alloc = frame_allocator.allocate_structured<ImGuiVertex>(vertices.size());
            data.vertex_alloc.upload(vertices);

            data.index_alloc = frame_allocator.allocate_structured<uint32_t>(indices.size());
            data.index_alloc.upload(indices);

            data.draw_data_alloc = frame_allocator.allocate_structured<ImGuiDrawData>(draw_data.size());
            data.draw_data_alloc.upload(draw_data);

            if (data.indirect_drawing)
            {
                data.indirect_alloc = frame_allocator.allocate_structured<DrawIndirectCommand>(m_draw_commands.size());
                data.indirect_alloc.upload(m_draw_commands);
            }
        },
        [this,
         &frame_allocator](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            record_pending_texture_ops(command, frame_allocator);

            // If there are no draw commands, just record pending texture ops
            if (m_draw_commands.empty())
                return;

            RenderPassInfo render_pass{};
            render_pass.color_attachments = {
                resources.get_framebuffer_attachment(data.output, LoadOperation::Load, StoreOperation::Store),
            };

            const RasterizationState rasterization_state{
                .cull_mode = RasterizationState::CullMode::None,
            };

            const DepthStencilState depth_stencil_state{
                .depth_test = false,
                .depth_write = false,
            };

            ColorBlendState color_blend_state{};
            color_blend_state.method = ColorBlendState::Method::PerAttachment;
            color_blend_state.attachments = {
                ColorBlendState::AttachmentState{
                    .blend_enabled = true,
                    .src_color_blend_factor = ColorBlendState::BlendFactor::SourceAlpha,
                    .dst_color_blend_factor = ColorBlendState::BlendFactor::OneMinusSourceAlpha,
                    .color_blend_op = ColorBlendState::BlendOperation::Add,
                    .src_alpha_blend_factor = ColorBlendState::BlendFactor::One,
                    .dst_alpha_blend_factor = ColorBlendState::BlendFactor::OneMinusSourceAlpha,
                    .alpha_blend_op = ColorBlendState::BlendOperation::Add,
                },
            };

            const auto pipeline = get_graphics_pipeline(
                ImGuiShaderVS{},
                ImGuiShaderFS{},
                rasterization_state,
                depth_stencil_state,
                color_blend_state,
                create_framebuffer_info(render_pass));

            command.begin_render_pass(render_pass);
            command.bind_pipeline(pipeline);

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(ImGuiDrawLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex) // g_vertices
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Vertex) // g_indices
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::Vertex) // g_draw_data
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)       // g_sampler
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const auto sampler = get_sampler_state({
                .minification_filter = ImageFilter::Linear,
                .magnification_filter = ImageFilter::Linear,
                .address_mode_u = ImageAddressMode::ClampToEdge,
                .address_mode_v = ImageAddressMode::ClampToEdge,
                .address_mode_w = ImageAddressMode::ClampToEdge,
            });

            const std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, data.vertex_alloc.view),
                WriteDescriptor::StructuredBufferSrv(1, data.index_alloc.view),
                WriteDescriptor::StructuredBufferSrv(2, data.draw_data_alloc.view),
                WriteDescriptor::SamplerState(0, sampler),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                ImGuiDrawLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.bind_descriptor_set(*descriptor_set, 0);
            command.bind_descriptor_set(*s_texture_pool->get_bindless_descriptor_set(), 1);

            struct PushConstant
            {
                uint32_t draw_data_idx;
            };

            if (data.indirect_drawing)
            {
                command.push_constant<PushConstant>({
                    .draw_data_idx = 0,
                });

                command.draw_indirect(
                    *frame_allocator.get_buffer(),
                    data.indirect_alloc.view.desc.offset,
                    static_cast<uint32_t>(m_draw_commands.size()),
                    sizeof(DrawIndirectCommand));
            }
            else
            {
                for (const DrawIndirectCommand& cmd : m_draw_commands)
                {
                    command.push_constant<PushConstant>({
                        .draw_data_idx = cmd.draw_index,
                    });

                    command.draw(cmd.vertex_count, 0);
                }
            }

            command.end_render_pass();
        });
}

void ImGuiRenderModule::rend_on_create(ImGuiHandle handle, const ImGuiStaticState& /*ss*/, const ImGuiDynamicState& ds)
{
    m_frame_data_list.push_back({
        .handle = handle,
        .snapshot = ds.frame,
    });

    process_texture_ops(ds.frame);
}

void ImGuiRenderModule::rend_on_update(ImGuiHandle handle, const ImGuiDynamicState& ds)
{
    auto it = std::find_if(m_frame_data_list.begin(), m_frame_data_list.end(), [handle](const ImGuiFrameData& data) {
        return handle == data.handle;
    });

    if (it == m_frame_data_list.end())
    {
        MIZU_UNREACHABLE("Trying to update invalid ImGui handle: {}", handle.get_internal_id());
        return;
    }

    it->snapshot = ds.frame;

    process_texture_ops(ds.frame);
}

void ImGuiRenderModule::rend_on_destroy(ImGuiHandle handle)
{
    std::erase_if(m_frame_data_list, [handle](const ImGuiFrameData& data) { return data.handle == handle; });
}

void ImGuiRenderModule::process_texture_ops(const ImGuiFrameSnapshot* snapshot)
{
    if (snapshot == nullptr || !snapshot->valid)
    {
        return;
    }

    for (const ImGuiTextureOp& op : snapshot->texture_ops)
    {
        switch (op.kind)
        {
        case ImGuiTextureOpKind::Create: {
            ImageDescription desc{};
            desc.width = op.texture_size.x;
            desc.height = op.texture_size.y;
            desc.depth = 1;
            desc.format = op.format;
            desc.usage = ImageUsageBits::TransferDst | ImageUsageBits::Sampled;

            if (!s_texture_pool->create(op.texture_id, desc))
                continue;

            ImageResource* resource = s_texture_pool->get_texture(op.texture_id);
            if (resource == nullptr)
                continue;

            m_pending_texture_ops.push_back({
                .resource = resource,
                .op = op,
            });

            break;
        }
        case ImGuiTextureOpKind::Update: {
            ImageResource* resource = s_texture_pool->get_texture(op.texture_id);
            if (resource == nullptr)
                continue;

            m_pending_texture_ops.push_back({
                .resource = resource,
                .op = op,
            });

            break;
        }
        case ImGuiTextureOpKind::Destroy: {
            std::erase_if(m_pending_texture_ops, [&op](const PendingTextureOp& pending) {
                return pending.op.texture_id == op.texture_id;
            });

            s_texture_pool->destroy(op.texture_id);
            break;
        }
        }
    }
}

void ImGuiRenderModule::record_pending_texture_ops(CommandBuffer& command, FrameLinearAllocator& frame_allocator)
{
    if (m_pending_texture_ops.empty())
    {
        return;
    }

    const DeviceProperties& properties = g_render_device->get_properties();
    const uint64_t row_pitch_alignment = properties.alignments.min_texture_row_pitch;
    const uint64_t placement_alignment = properties.alignments.min_texture_data_placement;

    for (const PendingTextureOp& pending : m_pending_texture_ops)
    {
        const ImGuiTextureOp& op = pending.op;
        ImageResource& image = *pending.resource;

        const uint32_t format_size = get_image_format_size(image.get_format());
        const uint64_t src_row_bytes = static_cast<uint64_t>(op.size.x) * format_size;
        const uint64_t row_pitch = math::align_up_pow2(src_row_bytes, row_pitch_alignment);

        MIZU_ASSERT(
            op.pixels.size() == src_row_bytes * op.size.y,
            "ImGui texture op pixel data size {} does not match expected size {}",
            op.pixels.size(),
            src_row_bytes * op.size.y);

        const FrameAllocation allocation =
            frame_allocator.allocate(row_pitch * op.size.y, placement_alignment, sizeof(uint8_t));

        uint8_t* mapped = allocation.get_mapped_data();
        for (uint32_t row = 0; row < op.size.y; ++row)
        {
            memcpy(mapped + row * row_pitch, op.pixels.data() + row * src_row_bytes, src_row_bytes);
        }

        const ImageResourceState old_state =
            op.kind == ImGuiTextureOpKind::Create ? ImageResourceState::Undefined : ImageResourceState::ShaderReadOnly;
        command.transition_resource(image, old_state, ImageResourceState::TransferDst);

        const CopyBufferToImageInfo info{
            .buffer_offset = allocation.view.desc.offset,
            .buffer_row_length = static_cast<uint32_t>(row_pitch / format_size),
            .image_subresource_layers =
                {
                    .mip_level = 0,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
            .image_offset = {op.offset.x, op.offset.y, 0},
            .image_extent = {op.size.x, op.size.y, 1},
        };

        command.copy_buffer_to_image(*frame_allocator.get_buffer(), image, info);

        command.transition_resource(image, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);
    }

    m_pending_texture_ops.clear();
}

} // namespace Mizu
