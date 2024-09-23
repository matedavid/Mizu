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
class ComputePipeline;
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
        RGRenderFunction func;
    };

    struct RGComputePass {
        std::shared_ptr<ComputePipeline> compute_pipeline;
        std::vector<size_t> resource_ids;
        RGComputeFunction func;
    };

    using RGPassT = std::variant<RGRenderPass, RGComputePass>;
    std::vector<RGPassT> m_passes;

    std::vector<std::shared_ptr<ResourceGroup>> m_resource_groups;
    std::unordered_map<size_t, size_t> m_id_to_resource_group;

    using ResourceMemberInfoT = std::variant<std::shared_ptr<Texture2D>, std::shared_ptr<UniformBuffer>>;
    struct RGResourceMemberInfo {
        std::string name;
        uint32_t set;
        ResourceMemberInfoT value;
    };

    void execute(const RGRenderPass& pass) const;
    void execute(const RGComputePass& pass) const;

    [[nodiscard]] static std::vector<size_t> create_resources(RenderGraph& rg,
                                                              const std::vector<ShaderDeclarationMemberInfo>& members,
                                                              const std::shared_ptr<IShader>& shader);

    [[nodiscard]] std::vector<size_t> create_render_pass_resources(const std::vector<RGResourceMemberInfo>& members,
                                                                   const std::shared_ptr<IShader>& shader);

    [[nodiscard]] static size_t get_resource_members_checksum(const std::vector<RGResourceMemberInfo>& members);
};

} // namespace Mizu
