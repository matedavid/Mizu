#pragma once

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"

#include "streaming/texture_streaming_manager.h"

namespace Mizu
{

class BufferResource;
class DescriptorSet;
class IAssetLoader;
class ImageResource;

class MaterialSystem
{
  public:
    MaterialSystem(TextureStreamingManager& texture_streaming_manager, IAssetLoader& asset_loader);

    void update();

    void register_material(
        MaterialAssetHandle handle,
        TransformHandle transform_handle,
        MeshAssetHandle mesh_handle,
        uint64_t identifier);
    void unregister_material(MaterialAssetHandle handle, uint64_t identifier);

    std::optional<uint32_t> get_texture_bindless_slot(TextureAssetHandle handle) const;
    std::optional<uint32_t> get_material_slot(MaterialAssetHandle handle) const;

    bool is_drawable(MaterialAssetHandle handle) const;

    std::shared_ptr<DescriptorSet> get_bindless_texture_descriptor_set() const
    {
        return m_bindless_texture_descriptor_set;
    }
    std::shared_ptr<BufferResource> get_material_buffer() const { return m_material_buffer; }

  private:
    TextureStreamingManager& m_texture_streaming_manager;
    IAssetLoader& m_asset_loader;

    struct TextureBinding
    {
        uint32_t bindless_slot = 0;
    };

    struct MaterialBinding
    {
        MaterialAssetMetadata metadata{};
        uint32_t buffer_slot = 0;
        uint32_t refcount = 0;
        ShaderAssetHandle shader_handle{};
    };

    std::shared_ptr<DescriptorSet> m_bindless_texture_descriptor_set{};
    std::vector<uint32_t> m_free_bindless_slots{};
    std::unordered_map<TextureAssetHandle, TextureBinding> m_texture_to_binding_map{};

    std::shared_ptr<BufferResource> m_material_buffer{};
    std::vector<uint32_t> m_free_material_buffer_slots{};
    std::unordered_map<MaterialAssetHandle, MaterialBinding> m_material_to_binding_map{};

    std::shared_ptr<ImageResource> m_default_texture{};

    void handle_texture_streaming_update(const TextureStreamingUpdate& update);

    uint32_t acquire_texture_binding(TextureAssetHandle handle);

    std::optional<uint32_t> allocate_bindless_descriptor_slot();
    void free_bindless_descriptor_slot(uint32_t slot);

    std::optional<uint32_t> allocate_material_buffer_slot();
    void free_material_buffer_slot(uint32_t slot);
};

extern MaterialSystem* g_material_system;

} // namespace Mizu