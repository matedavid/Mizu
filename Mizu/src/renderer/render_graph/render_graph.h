#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "renderer/render_graph/render_graph_builder.h"

namespace Mizu {

// Forward declarations
class RenderGraphBuilder;
class RenderCommandBuffer;
class RenderPass;
class ResourceGroup;
class GraphicsPipeline;
struct CommandBufferSubmitInfo;

class RenderGraph {
  public:
    static std::optional<RenderGraph> build(const RenderGraphBuilder& builder);

    void execute(const CommandBufferSubmitInfo& submit_info) const;

  private:
    std::shared_ptr<RenderCommandBuffer> m_command_buffer;

    struct RGRenderPass {
        std::shared_ptr<RenderPass> render_pass;
        std::shared_ptr<GraphicsPipeline> graphics_pipeline;
        std::vector<size_t> resource_ids;
        RGFunction func;
    };
    std::vector<RGRenderPass> m_render_passes;

    std::vector<std::shared_ptr<ResourceGroup>> m_resource_groups;
    std::unordered_map<size_t, size_t> m_id_to_resource_group;

    using ResourceMemberInfoT = std::variant<std::shared_ptr<Texture2D>, std::shared_ptr<UniformBuffer>>;
    struct RGResourceMemberInfo {
        std::string name;
        uint32_t set;
        ResourceMemberInfoT value;
    };

    [[nodiscard]] std::vector<size_t> create_render_pass_resources(const std::vector<RGResourceMemberInfo>& members,
                                                                   const std::shared_ptr<GraphicsShader>& shader);

    [[nodiscard]] static size_t get_resource_members_checksum(const std::vector<RGResourceMemberInfo>& members);
};

} // namespace Mizu
