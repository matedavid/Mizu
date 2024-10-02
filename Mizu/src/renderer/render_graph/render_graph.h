#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "renderer/render_graph/render_graph_builder.h"
#include "renderer/render_graph/render_graph_dependencies.h"

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
    ~RenderGraph();

    static std::optional<RenderGraph> build(const RenderGraphBuilder& builder);

    void execute(const CommandBufferSubmitInfo& submit_info) const;

  private:
    std::shared_ptr<RenderCommandBuffer> m_command_buffer;

    struct RGRenderPass {
        std::shared_ptr<RenderPass> render_pass;
        std::shared_ptr<GraphicsPipeline> graphics_pipeline;
        std::vector<size_t> resource_ids;
        RenderGraphDependencies dependencies;
        RGFunction func;
    };

    struct RGComputePass {
        std::string name;
        std::shared_ptr<ComputePipeline> compute_pipeline;
        std::vector<size_t> resource_ids;
        RenderGraphDependencies dependencies;
        RGFunction func;
    };

    struct RGResourceTransitionPass {
        std::shared_ptr<Texture2D> texture;
        ImageResourceState old_state;
        ImageResourceState new_state;
    };

    using RGPassT = std::variant<RGRenderPass, RGComputePass, RGResourceTransitionPass>;
    std::vector<RGPassT> m_passes;

    std::vector<std::shared_ptr<ResourceGroup>> m_resource_groups;
    std::unordered_map<size_t, size_t> m_id_to_resource_group;

    void execute(const RGRenderPass& pass) const;
    void execute(const RGComputePass& pass) const;
    void execute(const RGResourceTransitionPass& pass) const;

    //
    // RenderGraph Building
    //
    struct TextureUsage {
        enum class Type {
            SampledDependency,
            StorageDependency,
            Attachment,
        };

        Type type;
        size_t render_pass_pos = 0;
    };

    [[nodiscard]] static std::vector<TextureUsage> get_texture_usages(RGTextureRef texture,
                                                                      const RenderGraphBuilder& builder);

    using ResourceMemberInfoT = std::variant<std::shared_ptr<Texture2D>, std::shared_ptr<UniformBuffer>>;
    struct RGResourceMemberInfo {
        std::string name;
        uint32_t set;
        ResourceMemberInfoT value;
    };

    [[nodiscard]] std::vector<size_t> create_render_pass_resources(const std::vector<RGResourceMemberInfo>& members,
                                                                   const std::shared_ptr<IShader>& shader);

    [[nodiscard]] static size_t get_resource_members_checksum(const std::vector<RGResourceMemberInfo>& members);
};

} // namespace Mizu
