#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "render/resources/gpu_resource_types.h"
#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

class BufferResource;
class FrameLinearAllocator;
class RenderGraphBuilder;

class TransformRegistry : public TransformStateManagerConsumer
{
  public:
    TransformRegistry();
    ~TransformRegistry() override;

    void add_transform_publish_pass(RenderGraphBuilder& builder, FrameLinearAllocator& linear_allocator);

    // TransformStateManagerConsumer
    void rend_on_create(TransformHandle handle, const TransformStaticState& ss, const TransformDynamicState& ds)
        override;
    void rend_on_update(TransformHandle handle, const TransformDynamicState& ds) override;
    void rend_on_destroy(TransformHandle handle) override;

    const TransformInfo& get_transform_info(TransformHandle handle) const;
    const TransformInfo& get_transform_info(uint32_t slot_index) const;
    uint32_t get_transform_slot_index(TransformHandle handle) const;
    std::shared_ptr<BufferResource> get_transform_info_buffer() const { return m_transform_info_buffer; }

  private:
    static constexpr uint32_t INVALID_SLOT_U32 = std::numeric_limits<uint32_t>::max();

    struct TransformInfoSlot
    {
        TransformInfo transform_info{};
        bool occupied = false;
    };

    struct PendingTransformUpdate
    {
        TransformInfo new_transform{};
        uint32_t dst_slot = INVALID_SLOT_U32;

        uint32_t _padding[3]{};
    };

    std::vector<TransformInfoSlot> m_transform_infos{};
    std::vector<PendingTransformUpdate> m_pending_transform_updates{};

    std::shared_ptr<BufferResource> m_transform_info_buffer{};
};

void transform_registry_init();
void transform_registry_shutdown();
TransformRegistry& transform_registry_get();
void transform_registry_add_transform_publish_pass(RenderGraphBuilder& builder, FrameLinearAllocator& linear_allocator);
std::shared_ptr<BufferResource> transform_registry_get_transform_info_buffer();

} // namespace Mizu