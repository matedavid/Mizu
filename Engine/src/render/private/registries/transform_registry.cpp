#include "registries/transform_registry.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render.pipeline/scene_shaders.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/systems/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"

namespace Mizu
{

namespace
{

TransformInfo build_transform_info(const TransformDynamicState& ds)
{
    glm::mat4 transform{1.0f};
    transform = glm::translate(transform, ds.translation);
    transform = glm::rotate(transform, ds.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    transform = glm::rotate(transform, ds.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::rotate(transform, ds.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::scale(transform, ds.scale);

    return {
        .transform = transform,
        .normal_matrix = glm::transpose(glm::inverse(transform)),
    };
}
constexpr uint64_t NUM_TRANSFORMS = TransformConfig::MaxNumHandles;

} // namespace

TransformRegistry::TransformRegistry()
{
    g_transform_state_manager->register_rend_consumer(this);

    m_transform_infos.resize(NUM_TRANSFORMS);
    m_pending_transform_updates.reserve(NUM_TRANSFORMS);

    BufferDescription transform_info_buffer_desc{};
    transform_info_buffer_desc.size = sizeof(TransformInfo) * NUM_TRANSFORMS;
    transform_info_buffer_desc.stride = sizeof(TransformInfo);
    transform_info_buffer_desc.usage = BufferUsageBits::ShaderResource | BufferUsageBits::UnorderedAccess;
    transform_info_buffer_desc.name = "SceneSystem::TransformInfoBuffer";
    m_transform_info_buffer = g_render_device->create_buffer(transform_info_buffer_desc);
}

TransformRegistry::~TransformRegistry()
{
    g_transform_state_manager->unregister_rend_consumer(this);
}

void TransformRegistry::add_transform_publish_pass(RenderGraphBuilder& builder, FrameLinearAllocator& linear_allocator)
{
    if (m_pending_transform_updates.empty())
        return;

    const uint64_t pending_updates = m_pending_transform_updates.size();

    const FrameAllocation transform_info_buffer_allocation =
        linear_allocator.allocate_structured<PendingTransformUpdate>(pending_updates);
    transform_info_buffer_allocation.upload(std::span(m_pending_transform_updates.data(), pending_updates));

    struct PublishInfo
    {
        RenderGraphResource transform_buffer;
    };

    const RenderGraphResource transform_buffer_resource = builder.register_external_buffer(
        m_transform_info_buffer, {BufferResourceState::ShaderReadOnly, BufferResourceState::ShaderReadOnly});

    builder.add_pass<PublishInfo>(
        "SceneSystem::TransformPublishPass",
        [&](RenderGraphPassBuilder& pass, PublishInfo& info) {
            pass.set_hint(RenderGraphPassHint::Compute);

            info.transform_buffer = pass.write(transform_buffer_resource);
        },
        [=](CommandBuffer& command, const PublishInfo& info, const RenderGraphPassResources& resources) {
            struct PushConstant
            {
                uint64_t update_count;
            };

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TransformPublishLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(0, 1, ShaderType::Compute)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::shared_ptr<BufferResource>& transform_buffer = resources.get_buffer(info.transform_buffer);

            const std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, transform_info_buffer_allocation.view),
                WriteDescriptor::StructuredBufferUav(0, BufferResourceView::create(transform_buffer)),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                TransformPublishLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            const auto pipeline = get_compute_pipeline(PublishTransformsShaderCS{});
            command.bind_pipeline(pipeline);

            command.bind_descriptor_set(*descriptor_set, 0);
            command.push_constant<PushConstant>({
                .update_count = pending_updates,
            });

            const glm::uvec3 group_count =
                compute_group_count({pending_updates, 1, 1}, {PublishTransformsShaderCS::GROUP_SIZE, 1, 1});
            command.dispatch(group_count);
        });

    m_pending_transform_updates.clear();
}

void TransformRegistry::rend_on_create(
    TransformHandle handle,
    const TransformStaticState&,
    const TransformDynamicState& ds)
{
    const uint64_t id = handle.get_internal_id();

    const TransformInfo transform_info = build_transform_info(ds);

    TransformInfoSlot& slot = m_transform_infos[id];
    MIZU_ASSERT(!slot.occupied, "Trying to create transform info for an already occupied slot");

    slot.occupied = true;
    slot.transform_info = transform_info;

    m_pending_transform_updates.push_back({
        .new_transform = transform_info,
        .dst_slot = static_cast<uint32_t>(id),
    });
}

void TransformRegistry::rend_on_update(TransformHandle handle, const TransformDynamicState& ds)
{
    const uint64_t id = handle.get_internal_id();

    const TransformInfo transform_info = build_transform_info(ds);

    TransformInfoSlot& slot = m_transform_infos[id];
    MIZU_ASSERT(slot.occupied, "Trying to create transform info for an already occupied slot");

    slot.transform_info = transform_info;

    m_pending_transform_updates.push_back({
        .new_transform = transform_info,
        .dst_slot = static_cast<uint32_t>(id),
    });
}

void TransformRegistry::rend_on_destroy(TransformHandle handle)
{
    const uint64_t id = handle.get_internal_id();

    TransformInfoSlot& slot = m_transform_infos[id];
    MIZU_ASSERT(slot.occupied, "Trying to destroy transform info for an unoccupied slot");

    slot.occupied = false;
    slot.transform_info = {};

    m_pending_transform_updates.push_back({
        .new_transform = {},
        .dst_slot = static_cast<uint32_t>(id),
    });
}

const TransformInfo& TransformRegistry::get_transform_info(TransformHandle handle) const
{
    MIZU_ASSERT(handle.is_valid(), "Invalid TransformHandle");

    const uint64_t id = handle.get_internal_id();
    MIZU_ASSERT(id < m_transform_infos.size(), "Trying to get transform info for an invalid handle");

    const TransformInfoSlot& slot = m_transform_infos[id];
    MIZU_ASSERT(slot.occupied, "Trying to get transform info for an unoccupied slot");

    return slot.transform_info;
}

const TransformInfo& TransformRegistry::get_transform_info(uint32_t slot_index) const
{
    MIZU_ASSERT(slot_index < m_transform_infos.size(), "Trying to get transform info for an invalid slot index");
    return m_transform_infos[std::min(static_cast<uint64_t>(slot_index), NUM_TRANSFORMS - 1)].transform_info;
}

uint32_t TransformRegistry::get_transform_slot_index(TransformHandle handle) const
{
    const uint64_t id = handle.get_internal_id();

    const TransformInfoSlot& slot = m_transform_infos[id];
    if (!slot.occupied)
    {
        MIZU_ASSERT(false, "Trying to get transform slot index for an unoccupied slot");
        return INVALID_SLOT_U32;
    }

    return static_cast<uint32_t>(id);
}

static TransformRegistry* s_transform_registry = nullptr;

void transform_registry_init()
{
    MIZU_ASSERT(s_transform_registry == nullptr, "TransformRegistry is already initialized");
    s_transform_registry = new TransformRegistry{};
}

void transform_registry_shutdown()
{
    delete s_transform_registry;
    s_transform_registry = nullptr;
}

TransformRegistry& transform_registry_get()
{
    MIZU_ASSERT(s_transform_registry != nullptr, "TransformRegistry is not initialized");
    return *s_transform_registry;
}

void transform_registry_add_transform_publish_pass(RenderGraphBuilder& builder, FrameLinearAllocator& linear_allocator)
{
    MIZU_ASSERT(s_transform_registry != nullptr, "TransformRegistry is not initialized");
    s_transform_registry->add_transform_publish_pass(builder, linear_allocator);
}

std::shared_ptr<BufferResource> transform_registry_get_transform_info_buffer()
{
    MIZU_ASSERT(s_transform_registry != nullptr, "TransformRegistry is not initialized");
    return s_transform_registry->get_transform_info_buffer();
}

} // namespace Mizu
