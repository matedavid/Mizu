#include "imgui/imgui_context.h"

#include <array>
#include <backends/imgui_impl_glfw.h>
#include <cstring>

#include "base/debug/assert.h"

#include "core/runtime.h"
#include "core/window.h"

#include "imgui/imgui_panels.h"
#include "imgui/imgui_state_manager.h"
#include "imgui/imgui_types.h"

namespace Mizu
{

namespace
{

constexpr uint64_t FrameSnapshotCount = MaxTicksAhead + 1;

std::array<ImGuiFrameSnapshot, FrameSnapshotCount> s_frame_snapshots{};
uint64_t s_frame_snapshot_idx = 0;

ImGuiHandle s_handle{};
bool s_enabled = true;
ImGuiTextureId s_next_texture_id = IMGUI_INVALID_TEXTURE_ID + 1;

ImageFormat convert_texture_format(ImTextureFormat format)
{
    switch (format)
    {
    case ImTextureFormat_RGBA32:
        return ImageFormat::R8G8B8A8_UNORM;
    case ImTextureFormat_Alpha8:
        MIZU_UNREACHABLE("Not implemented");
        break;
    }

    MIZU_UNREACHABLE("Invalid ImTextureFormat");
    return ImageFormat::R8G8B8A8_UNORM;
}

ImGuiTextureOp make_texture_op(
    ImGuiTextureOpKind kind,
    ImGuiTextureId texture_id,
    ImTextureData& texture,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height)
{
    ImGuiTextureOp op{};
    op.kind = kind;
    op.texture_id = texture_id;
    op.format = convert_texture_format(texture.Format);
    op.texture_size = glm::uvec2(texture.Width, texture.Height);
    op.offset = glm::uvec2(x, y);
    op.size = glm::uvec2(width, height);

    const uint64_t row_size = static_cast<uint64_t>(width) * static_cast<uint64_t>(texture.BytesPerPixel);
    op.pixels.resize(row_size * height);

    for (uint32_t row = 0; row < height; ++row)
    {
        const void* src = texture.GetPixelsAt(static_cast<int>(x), static_cast<int>(y + row));
        memcpy(op.pixels.data() + row * row_size, src, row_size);
    }

    return op;
}

void process_texture(ImTextureData& texture, std::vector<ImGuiTextureOp>& texture_ops)
{
    switch (texture.Status)
    {
    case ImTextureStatus_WantCreate: {
        const ImGuiTextureId texture_id = s_next_texture_id++;

        texture_ops.push_back(make_texture_op(
            ImGuiTextureOpKind::Create,
            texture_id,
            texture,
            0,
            0,
            static_cast<uint32_t>(texture.Width),
            static_cast<uint32_t>(texture.Height)));

        texture.SetTexID(static_cast<ImTextureID>(texture_id));
        texture.SetStatus(ImTextureStatus_OK);
        break;
    }
    case ImTextureStatus_WantUpdates: {
        const ImTextureRect& rect = texture.UpdateRect;

        texture_ops.push_back(make_texture_op(
            ImGuiTextureOpKind::Update,
            static_cast<ImGuiTextureId>(texture.TexID),
            texture,
            rect.x,
            rect.y,
            rect.w,
            rect.h));

        texture.SetStatus(ImTextureStatus_OK);
        break;
    }
    case ImTextureStatus_WantDestroy: {
        ImGuiTextureOp op{};
        op.kind = ImGuiTextureOpKind::Destroy;
        op.texture_id = static_cast<ImGuiTextureId>(texture.TexID);
        texture_ops.push_back(op);

        texture.SetTexID(ImTextureID_Invalid);
        texture.SetStatus(ImTextureStatus_Destroyed);
        break;
    }
    case ImTextureStatus_OK:
    case ImTextureStatus_Destroyed:
        break;
    }
}

void copy_draw_list(const ImDrawList& src, ImGuiDrawList& dst)
{
    dst.commands.clear();
    dst.vertices.clear();
    dst.indices.clear();

    dst.vertices.reserve(static_cast<size_t>(src.VtxBuffer.Size));
    for (const ImDrawVert& vertex : src.VtxBuffer)
    {
        dst.vertices.push_back({
            .position = glm::vec2(vertex.pos.x, vertex.pos.y),
            .uv = glm::vec2(vertex.uv.x, vertex.uv.y),
            .color = vertex.col,
        });
    }

    dst.indices.assign(src.IdxBuffer.begin(), src.IdxBuffer.end());

    dst.commands.reserve(static_cast<size_t>(src.CmdBuffer.Size));
    for (const ImDrawCmd& cmd : src.CmdBuffer)
    {
        if (cmd.UserCallback != nullptr)
            continue;

        dst.commands.push_back({
            .clip_rect = glm::vec4(cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w),
            .texture_id = static_cast<ImGuiTextureId>(cmd.GetTexID()),
            .vertex_offset = cmd.VtxOffset,
            .index_offset = cmd.IdxOffset,
            .element_count = cmd.ElemCount,
        });
    }
}

void build_snapshot(ImDrawData& data, ImGuiFrameSnapshot& snapshot)
{
    snapshot.valid = data.Valid;

    if (!snapshot.valid)
        return;

    snapshot.cmd_lists.clear();
    snapshot.texture_ops.clear();

    snapshot.display_pos = glm::vec2(data.DisplayPos.x, data.DisplayPos.y);
    snapshot.display_size = glm::vec2(data.DisplaySize.x, data.DisplaySize.y);
    snapshot.framebuffer_scale = glm::vec2(data.FramebufferScale.x, data.FramebufferScale.y);

    if (data.Textures != nullptr)
    {
        for (ImTextureData* texture : *data.Textures)
            process_texture(*texture, snapshot.texture_ops);
    }

    snapshot.cmd_lists.resize(static_cast<size_t>(data.CmdLists.Size));
    for (int i = 0; i < data.CmdLists.Size; ++i)
        copy_draw_list(*data.CmdLists[i], snapshot.cmd_lists[static_cast<size_t>(i)]);
}

} // namespace

void ImGuiContext::init(const Window& window)
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // IF using Docking Branch

    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window.handle(), true);

    g_imgui_state_manager = new ImGuiStateManager{};
    g_state_manager_coordinator->register_state_manager(StateManagerRegistrationBuilder::begin(g_imgui_state_manager));
}

void ImGuiContext::shutdown()
{
    s_handle = ImGuiHandle{};
    delete g_imgui_state_manager;
    g_imgui_state_manager = nullptr;

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiContext::init_simulation_state()
{
    s_handle = g_imgui_state_manager->sim_create(ImGuiStaticState{}, ImGuiDynamicState{});
}

void ImGuiContext::new_frame()
{
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiContext::render()
{
    if (s_enabled)
        ImGuiPanels::execute();

    ImGui::Render();

    ImGuiFrameSnapshot& snapshot = s_frame_snapshots[s_frame_snapshot_idx % FrameSnapshotCount];
    s_frame_snapshot_idx += 1;

    build_snapshot(*ImGui::GetDrawData(), snapshot);

    g_imgui_state_manager->sim_update(s_handle, ImGuiDynamicState{.frame = &snapshot});
}

void ImGuiContext::set_enabled(bool enabled)
{
    s_enabled = enabled;
}

bool ImGuiContext::is_enabled()
{
    return s_enabled;
}

} // namespace Mizu
