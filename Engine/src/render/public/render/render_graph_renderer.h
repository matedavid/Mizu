#pragma once

#include <memory>
#include <vector>

#include "render/draw_block_manager.h"
#include "render/runtime/game_renderer.h"

namespace Mizu
{

// Forward declarations
class BufferResource;
class Camera;
class ImageResource;
class MaterialResidencySystem;
class Mesh;
class RenderGraphBlackboard;
class TextureResidencySystem;

class RenderGraphRenderer : public IRenderModule
{
  public:
    RenderGraphRenderer();

    void set_gpu_mesh_pool(GpuMeshPool* gpu_mesh_pool) override { m_gpu_mesh_pool = gpu_mesh_pool; }

    void set_texture_residency_system(TextureResidencySystem* texture_residency) override
    {
        m_texture_residency_system = texture_residency;
    }
    void set_material_residency_system(MaterialResidencySystem* material_residency) override
    {
        m_material_residency_system = material_residency;
    }

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) override;

  private:
    // Meshes info
    static constexpr size_t TRANSFORM_INFO_BUFFER_SIZE = 1000;
    std::vector<InstanceTransformInfo> m_transform_info_buffer;

    std::vector<uint64_t> m_main_view_transform_indices_buffer;
    std::vector<uint64_t> m_shadows_view_transform_indices_buffer;

    // Misc
    std::shared_ptr<BufferResource> m_fullscreen_triangle;

    std::unique_ptr<DrawBlockManager> m_draw_manager;

    // TODO - TEMPORARY until we create the dispatch mechanism instead of making render modules do the dispatching
    GpuMeshPool* m_gpu_mesh_pool = nullptr;
    TextureResidencySystem* m_texture_residency_system = nullptr;
    MaterialResidencySystem* m_material_residency_system = nullptr;

    void render_scene(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_depth_normals_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_light_culling_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void get_light_information(RenderGraphBlackboard& blackboard);
    void create_draw_lists(RenderGraphBlackboard& blackboard);
};

} // namespace Mizu
