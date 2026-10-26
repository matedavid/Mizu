#include "resources/material_system.h"

#include <algorithm>
#include <array>
#include <optional>

#include "asset/asset_loader.h"
#include "asset/asset_metadata.h"
#include "base/debug/assert.h"
#include "base/utils/hash.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/descriptors.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_view.h"

#include "render/runtime/renderer.h"
#include "render/utils/image_utils.h"

namespace Mizu
{

MaterialSystem* g_material_system = nullptr;

namespace
{

TextureStreamingHandle get_streaming_handle(
    uint64_t identifier,
    MaterialAssetHandle material,
    TextureAssetHandle texture,
    uint64_t i)
{
    return hash_compute(identifier, material.get_id(), texture.get_id(), i);
}

} // namespace

MaterialSystem::MaterialSystem(TextureStreamingManager& texture_streaming_manager, IAssetLoader& asset_loader)
    : m_texture_streaming_manager(texture_streaming_manager)
    , m_asset_loader(asset_loader)
{
    // Textures

    static constexpr uint32_t NUM_BINDLESS_TEXTURES = 1'000;

    const std::array bindless_texture_layout = {
        DescriptorItem::TextureSrv(0, BINDLESS_DESCRIPTOR_COUNT, ShaderType::Fragment),
    };

    const DescriptorSetLayoutHandle bindless_texture_layout_handle =
        g_render_device->create_descriptor_set_layout({.layout = bindless_texture_layout});

    m_bindless_texture_descriptor_set = g_render_device->allocate_descriptor_set(
        bindless_texture_layout_handle, DescriptorSetAllocationType::Bindless, NUM_BINDLESS_TEXTURES);

    m_free_bindless_slots.resize(NUM_BINDLESS_TEXTURES);
    // Starting from 1 because slot 0 is reserved for the default texture
    std::iota(m_free_bindless_slots.rbegin(), m_free_bindless_slots.rend(), 1);

    ImageDescription default_desc{};
    default_desc.width = 1;
    default_desc.height = 1;
    default_desc.type = ImageType::Image2D;
    default_desc.format = ImageFormat::R8G8B8A8_UNORM;
    default_desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    default_desc.name = "MaterialSystem::DefaultTexture";

    uint8_t default_data[] = {255, 192, 203, 255};
    m_default_texture = ImageUtils::create_texture2d(default_desc, default_data);

    const ImageResourceView default_srv = ImageResourceView::create(m_default_texture);

    std::vector<WriteDescriptor> default_writes{NUM_BINDLESS_TEXTURES};
    std::fill(default_writes.begin(), default_writes.end(), WriteDescriptor::TextureSrv(0, default_srv));

    m_bindless_texture_descriptor_set->update(default_writes, 0);

    // Materials

    m_free_material_buffer_slots.resize(StaticMeshConfig::MaxNumHandles);
    std::iota(m_free_material_buffer_slots.rbegin(), m_free_material_buffer_slots.rend(), 0);

    BufferDescription material_buffer_desc{};
    material_buffer_desc.size = StaticMeshConfig::MaxNumHandles * sizeof(uint32_t) * MAX_TEXTURES_PER_MATERIAL;
    material_buffer_desc.stride = sizeof(uint32_t);
    material_buffer_desc.usage = BufferUsageBits::ShaderResource | BufferUsageBits::HostVisible;
    material_buffer_desc.name = "MaterialSystem::MaterialBuffer";

    m_material_buffer = g_render_device->create_buffer(material_buffer_desc);
}

void MaterialSystem::update()
{
    m_pending_updates.clear();

    for (const TextureStreamingUpdate& update : m_texture_streaming_manager.get_updates())
    {
        handle_texture_streaming_update(update);
    }
}

void MaterialSystem::register_material(
    MaterialAssetHandle handle,
    TransformHandle transform_handle,
    MeshAssetHandle mesh_handle,
    uint64_t identifier)
{
    const std::optional<MaterialAssetRecord> material_record = m_asset_loader.get_material_record(handle);
    if (!material_record.has_value())
    {
        MIZU_UNREACHABLE("Failed to get material record for asset: {}", handle.get_id());
        return;
    }

    const std::optional<MeshAssetRecord> mesh_record = m_asset_loader.get_mesh_record(mesh_handle);
    if (!mesh_record.has_value())
    {
        MIZU_UNREACHABLE("Failed to get mesh record for asset: {}", mesh_handle.get_id());
        return;
    }

    const MaterialAssetMetadata& material_metadata = material_record->metadata;
    const MeshAssetMetadata& mesh_metadata = mesh_record->metadata;

    auto material_it = m_material_to_binding_map.find(handle);
    if (material_it == m_material_to_binding_map.end())
    {
        const std::optional<uint32_t> slot = allocate_material_buffer_slot();
        if (!slot.has_value())
        {
            MIZU_UNREACHABLE("Failed to allocate material buffer slot for {}", handle.get_id());
            return;
        }

        const MaterialBinding binding{
            .buffer_slot = *slot,
            .shader_handle = material_metadata.shader_handle,
        };

        const MaterialBindingInternal binding_internal{
            .binding = binding,
            .metadata = material_metadata,
            .drawable = is_drawable(handle),
            .refcount = 0, // Will get increased below
        };

        material_it = m_material_to_binding_map.emplace(handle, binding_internal).first;

        std::array<uint32_t, MAX_TEXTURES_PER_MATERIAL> texture_slots{};
        for (uint32_t i = 0; i < material_metadata.num_textures; ++i)
            texture_slots[i] = acquire_texture_binding(material_metadata.texture_handles[i], handle);

        uint8_t* mapped_data = m_material_buffer->get_mapped_data();
        MIZU_ASSERT(mapped_data != nullptr, "Failed to map material buffer");

        constexpr uint32_t STRIDE = sizeof(uint32_t) * MAX_TEXTURES_PER_MATERIAL;
        uint8_t* mapped_data_slot = std::next(mapped_data, *slot * STRIDE);

        memcpy(mapped_data_slot, texture_slots.data(), texture_slots.size() * sizeof(uint32_t));
    }

    material_it->second.refcount += 1;

    for (uint32_t i = 0; i < material_metadata.num_textures; ++i)
    {
        const TextureAssetHandle texture_handle = material_metadata.texture_handles[i];
        m_texture_streaming_manager.add_usage({
            .streaming_handle = get_streaming_handle(identifier, handle, texture_handle, i),
            .texture_handle = texture_handle,
            .transform_handle = transform_handle,
            .aabb = mesh_metadata.bounding_box,
            .texel_factor = mesh_metadata.texel_factor,
        });
    }
}

void MaterialSystem::unregister_material(MaterialAssetHandle handle, uint64_t identifier)
{
    const auto material_it = m_material_to_binding_map.find(handle);
    if (material_it == m_material_to_binding_map.end())
    {
        MIZU_UNREACHABLE("Trying to unregister material that was not previously registered: {}", handle.get_id());
        return;
    }

    const std::optional<MaterialAssetRecord> record = m_asset_loader.get_material_record(handle);
    if (!record.has_value())
    {
        MIZU_UNREACHABLE("Failed to get material record for asset: {}", handle.get_id());
        return;
    }

    const MaterialAssetMetadata& metadata = record->metadata;

    for (uint32_t i = 0; i < metadata.num_textures; ++i)
    {
        const TextureAssetHandle texture_handle = metadata.texture_handles[i];
        m_texture_streaming_manager.remove_usage(get_streaming_handle(identifier, handle, texture_handle, i));
    }

    material_it->second.refcount -= 1;
    if (material_it->second.refcount == 0)
    {
        for (uint32_t i = 0; metadata.num_textures; ++i)
        {
            const TextureAssetHandle texture_handle = metadata.texture_handles[i];

            auto texture_it = m_texture_to_binding_map.find(texture_handle);
            if (texture_it == m_texture_to_binding_map.end())
            {
                MIZU_UNREACHABLE("Could not find texture binding");
                continue;
            }

            std::erase_if(
                texture_it->second.reference_materials, [handle](MaterialAssetHandle h) { return h == handle; });
        }

        free_material_buffer_slot(material_it->second.binding.buffer_slot);
        m_material_to_binding_map.erase(material_it);

        m_pending_updates.push_back({
            .handle = handle,
            .binding = {},
            .drawable = false,
        });
    }
}

std::optional<TextureBinding> MaterialSystem::get_texture_binding(TextureAssetHandle handle) const
{
    const auto it = m_texture_to_binding_map.find(handle);
    if (it == m_texture_to_binding_map.end())
        return std::nullopt;

    return it->second.binding;
}

std::optional<MaterialBinding> MaterialSystem::get_material_binding(MaterialAssetHandle handle) const
{
    const auto it = m_material_to_binding_map.find(handle);
    if (it == m_material_to_binding_map.end())
        return std::nullopt;

    return it->second.binding;
}

bool MaterialSystem::is_drawable(MaterialAssetHandle handle) const
{
    const auto it = m_material_to_binding_map.find(handle);
    if (it == m_material_to_binding_map.end())
        return false;

    const MaterialAssetMetadata& metadata = it->second.metadata;

    for (uint32_t i = 0; i < metadata.num_textures; ++i)
    {
        if (!m_texture_streaming_manager.is_drawable(metadata.texture_handles[i]))
            return false;
    }

    return true;
}

void MaterialSystem::handle_texture_streaming_update(const TextureStreamingUpdate& update)
{
    auto it = m_texture_to_binding_map.find(update.handle);
    if (it == m_texture_to_binding_map.end())
    {
        MIZU_UNREACHABLE("Updating a texture that was not registered: {}", update.handle.get_id());
        return;
    }

    if (update.new_texture == nullptr)
    {
        free_bindless_descriptor_slot(it->second.binding.bindless_slot);
        m_texture_to_binding_map.erase(it);
    }
    else
    {
        const std::array descriptor_writes = {
            WriteDescriptor::TextureSrv(0, ImageResourceView::create(update.new_texture)),
        };
        m_bindless_texture_descriptor_set->update(descriptor_writes, it->second.binding.bindless_slot);
    }

    for (MaterialAssetHandle material : it->second.reference_materials)
    {
        update_material(material);
    }
}

void MaterialSystem::update_material(MaterialAssetHandle handle)
{
    auto material_it = m_material_to_binding_map.find(handle);

    if (material_it == m_material_to_binding_map.end())
    {
        MIZU_UNREACHABLE("Failed to get material");
        return;
    }

    const bool drawable = material_it->second.drawable;
    material_it->second.drawable = is_drawable(handle);

    if (drawable != material_it->second.drawable)
    {
        m_pending_updates.push_back({
            .handle = handle,
            .binding = material_it->second.binding,
            .drawable = material_it->second.drawable,
        });
    }
}

uint32_t MaterialSystem::acquire_texture_binding(TextureAssetHandle handle, MaterialAssetHandle material_handle)
{
    constexpr uint32_t DEFAULT_TEXTURE_SLOT = 0;

    auto it = m_texture_to_binding_map.find(handle);

    if (it == m_texture_to_binding_map.end())
    {
        const std::optional<uint32_t> bindless_slot = allocate_bindless_descriptor_slot();
        if (!bindless_slot.has_value())
        {
            MIZU_UNREACHABLE("Failed to allocate bindless descriptor slot for texture {}", handle.get_id());
            return DEFAULT_TEXTURE_SLOT;
        }

        const TextureBinding binding{
            .bindless_slot = *bindless_slot,
        };

        const TextureBindingInternal binding_internal{
            .binding = binding,
        };

        it = m_texture_to_binding_map.emplace(handle, binding_internal).first;
    }

    it->second.reference_materials.push_back(material_handle);

    return it->second.binding.bindless_slot;
}

std::optional<uint32_t> MaterialSystem::allocate_bindless_descriptor_slot()
{
    if (m_free_bindless_slots.empty())
        return std::nullopt;

    const uint32_t slot = m_free_bindless_slots.back();
    m_free_bindless_slots.pop_back();

    return slot;
}

void MaterialSystem::free_bindless_descriptor_slot(uint32_t slot)
{
    m_free_bindless_slots.push_back(slot);
}

std::optional<uint32_t> MaterialSystem::allocate_material_buffer_slot()
{
    if (m_free_material_buffer_slots.empty())
        return std::nullopt;

    const uint32_t slot = m_free_material_buffer_slots.back();
    m_free_material_buffer_slots.pop_back();

    return slot;
}

void MaterialSystem::free_material_buffer_slot(uint32_t slot)
{
    m_free_material_buffer_slots.push_back(slot);
}

} // namespace Mizu