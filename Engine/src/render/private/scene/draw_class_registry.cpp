#include "scene/draw_class_registry.h"

#include "base/debug/assert.h"

namespace Mizu
{

DrawClassId DrawClassRegistry::acquire(const DrawClassDesc& desc)
{
    m_total_live_count += 1;

    const auto it = m_lookup.find(desc);
    if (it != m_lookup.end())
    {
        Entry& entry = m_entries[it->second];
        entry.live_count += 1;

        if (entry.live_index == INVALID_LIVE_INDEX)
        {
            entry.live_index = static_cast<uint32_t>(m_live_classes.size());
            m_live_classes.push_back(it->second);
        }

        return it->second;
    }

    DrawClassId class_id = INVALID_DRAW_CLASS_ID;
    if (!m_free_ids.empty())
    {
        class_id = m_free_ids.back();
        m_free_ids.pop_back();
    }
    else
    {
        class_id = static_cast<DrawClassId>(m_entries.size());
        m_entries.emplace_back();
    }

    Entry& entry = m_entries[class_id];
    entry.desc = desc;
    entry.live_count = 1;
    entry.live_index = static_cast<uint32_t>(m_live_classes.size());

    m_live_classes.push_back(class_id);
    m_lookup.insert({desc, class_id});

    return class_id;
}

void DrawClassRegistry::release(DrawClassId class_id)
{
    MIZU_ASSERT(class_id < m_entries.size(), "Draw class id {} is out of range", class_id);

    Entry& entry = m_entries[class_id];
    MIZU_ASSERT(entry.live_count > 0, "Releasing draw class id {} that has no live drawables", class_id);

    entry.live_count -= 1;
    m_total_live_count -= 1;

    if (entry.live_count > 0)
        return;

    const uint32_t live_index = entry.live_index;
    MIZU_ASSERT(live_index < m_live_classes.size(), "Draw class id {} has an invalid live index", class_id);

    const DrawClassId moved = m_live_classes.back();
    m_live_classes[live_index] = moved;
    m_live_classes.pop_back();

    m_entries[moved].live_index = live_index;

    entry.live_index = INVALID_LIVE_INDEX;

    m_lookup.erase(entry.desc);
    entry.desc = DrawClassDesc{};

    m_free_ids.push_back(class_id);
}

const DrawClassDesc& DrawClassRegistry::get_desc(DrawClassId class_id) const
{
    MIZU_ASSERT(class_id < m_entries.size(), "Draw class id {} is out of range", class_id);
    return m_entries[class_id].desc;
}

uint32_t DrawClassRegistry::get_live_count(DrawClassId class_id) const
{
    MIZU_ASSERT(class_id < m_entries.size(), "Draw class id {} is out of range", class_id);
    return m_entries[class_id].live_count;
}

} // namespace Mizu
