#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "render_core/rhi/image_resource.h"

namespace Mizu
{

using ImGuiTextureId = uint64_t;
constexpr ImGuiTextureId IMGUI_INVALID_TEXTURE_ID = 0;

struct ImGuiVertex
{
    glm::vec2 position{};
    glm::vec2 uv{};
    uint32_t color = 0;

    uint32_t _padding = 0;
};

static_assert(sizeof(ImGuiVertex) == 24, "ImGuiVertex must match the std430 stride of the shader struct");

struct ImGuiDrawCommand
{
    glm::vec4 clip_rect{};
    ImGuiTextureId texture_id = IMGUI_INVALID_TEXTURE_ID;
    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0;
    uint32_t element_count = 0;
};

struct ImGuiDrawList
{
    std::vector<ImGuiDrawCommand> commands{};
    std::vector<uint32_t> indices{};
    std::vector<ImGuiVertex> vertices{};
};

enum class ImGuiTextureOpKind
{
    Create,
    Update,
    Destroy,
};

struct ImGuiTextureOp
{
    ImGuiTextureOpKind kind{};
    ImGuiTextureId texture_id = IMGUI_INVALID_TEXTURE_ID;
    ImageFormat format{};

    glm::uvec2 texture_size{};
    glm::uvec2 offset{};
    glm::uvec2 size{};
    std::vector<uint8_t> pixels{};
};

struct ImGuiFrameSnapshot
{
    bool valid = false;

    glm::vec2 display_pos{};
    glm::vec2 display_size{};
    glm::vec2 framebuffer_scale{};

    std::vector<ImGuiDrawList> cmd_lists{};
    std::vector<ImGuiTextureOp> texture_ops{};
};

} // namespace Mizu