#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include "render/scene/draw_class.h"

namespace Mizu
{

class DrawClassRegistry
{
  public:
    DrawClassId acquire(const DrawClassDesc& desc);
    void release(DrawClassId class_id);

    const DrawClassDesc& get_desc(DrawClassId class_id) const;
    uint32_t get_live_count(DrawClassId class_id) const;

    std::span<const DrawClassId> live_classes() const { return m_live_classes; }
    uint32_t num_entries() const { return static_cast<uint32_t>(m_entries.size()); }

    uint32_t total_live_count() const { return m_total_live_count; }

  private:
    struct Entry
    {
        DrawClassDesc desc{};
        uint32_t live_count = 0;
        uint32_t live_index = INVALID_LIVE_INDEX;
    };

    static constexpr uint32_t INVALID_LIVE_INDEX = std::numeric_limits<uint32_t>::max();

    struct DescHasher
    {
        size_t operator()(const DrawClassDesc& desc) const { return desc.hash(); }
    };

    std::vector<Entry> m_entries{};
    std::vector<DrawClassId> m_free_ids{};
    std::vector<DrawClassId> m_live_classes{};
    std::unordered_map<DrawClassDesc, DrawClassId, DescHasher> m_lookup{};

    uint32_t m_total_live_count = 0;
};

} // namespace Mizu
