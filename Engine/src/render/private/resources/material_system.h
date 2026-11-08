#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"
#include "render_core/rhi/resource_view.h"

#include "streaming/texture_streaming_manager.h"

#include <vulkan/vulkan_core.h>

namespace Mizu
{

class BufferResource;
class DescriptorSet;
class IAssetLoader;
class ImageResource;

struct TextureBinding
{
    uint32_t bindless_slot = 0;
};

struct MaterialBinding
{
    uint32_t buffer_offset = 0;
    ShaderAssetHandle shader_handle{};
};

struct MaterialUpdate
{
    MaterialAssetHandle handle{};
    MaterialBinding binding{};
    bool drawable = false;
};

class MaterialSystem
{
  public:
    MaterialSystem(
        TextureStreamingManager& texture_streaming_manager,
        IAssetLoader& asset_loader,
        uint32_t frames_in_flight);

    void update(uint32_t frame_in_flight_idx);

    void register_material(
        MaterialAssetHandle handle,
        TransformHandle transform_handle,
        MeshAssetHandle mesh_handle,
        uint64_t identifier);
    void unregister_material(MaterialAssetHandle handle, uint64_t identifier);

    std::optional<TextureBinding> get_texture_binding(TextureAssetHandle handle) const;
    std::optional<MaterialBinding> get_material_binding(MaterialAssetHandle handle) const;

    bool is_drawable(MaterialAssetHandle handle) const;
    std::span<const MaterialUpdate> get_updates() const { return m_pending_updates; }

    std::shared_ptr<DescriptorSet> get_bindless_descriptor_set(uint32_t frame_in_flight_idx) const
    {
        return m_bindless_texture_descriptor_sets[frame_in_flight_idx];
    }
    std::shared_ptr<BufferResource> get_material_buffer() const { return m_material_buffer; }

  private:
    TextureStreamingManager& m_texture_streaming_manager;
    IAssetLoader& m_asset_loader;
    uint32_t m_frames_in_flight = 0;

    struct TextureBindingInternal
    {
        TextureBinding binding{};

        // TODO: I don't like this :(
        std::vector<MaterialAssetHandle> reference_materials{};
    };

    struct MaterialBindingInternal
    {
        MaterialBinding binding{};
        uint32_t buffer_slot = 0;

        MaterialAssetMetadata metadata{};
        bool drawable = false;
        uint32_t refcount = 0;
    };

    struct PendingBindlessWrite
    {
        uint32_t bindless_slot = 0;
        ImageResourceView view{};
    };

    struct RetiredBindlessSlot
    {
        uint32_t bindless_slot = 0;
        uint32_t frames = 0;
    };

    // One bindless descriptor set per frame in flight
    std::vector<std::shared_ptr<DescriptorSet>> m_bindless_texture_descriptor_sets{};
    std::vector<std::vector<PendingBindlessWrite>> m_pending_bindless_writes{};

    std::vector<uint32_t> m_free_bindless_slots{};
    std::vector<RetiredBindlessSlot> m_retired_bindless_slots{};
    std::unordered_map<TextureAssetHandle, TextureBindingInternal> m_texture_to_binding_map{};

    std::shared_ptr<BufferResource> m_material_buffer{};
    std::vector<uint32_t> m_free_material_buffer_slots{};
    std::unordered_map<MaterialAssetHandle, MaterialBindingInternal> m_material_to_binding_map{};

    std::vector<MaterialUpdate> m_pending_updates{};

    std::shared_ptr<ImageResource> m_default_texture{};
    ImageResourceView m_default_texture_view{};

    void handle_texture_streaming_update(const TextureStreamingUpdate& update);
    void update_material(MaterialAssetHandle handle);

    uint32_t acquire_texture_binding(TextureAssetHandle handle, MaterialAssetHandle material_handle);

    void queue_bindless_write(uint32_t bindless_slot, const ImageResourceView& view);
    void apply_pending_bindless_writes(uint32_t frame_in_flight_idx);

    void update_retired_bindless_slots();
    void retire_bindless_slot(uint32_t bindless_slot);

    std::optional<uint32_t> allocate_bindless_descriptor_slot();
    void free_bindless_descriptor_slot(uint32_t slot);

    std::optional<uint32_t> allocate_material_buffer_slot();
    void free_material_buffer_slot(uint32_t slot);
};

} // namespace Mizu