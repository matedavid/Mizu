#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

#include "base/containers/inplace_vector.h"
#include "base/debug/logging.h"
#include "state_manager/base_state_manager.h"
#include "state_manager/state_manager_consumer.h"

#include "mizu_render_module.h"
#include "render/render_settings/render_settings.h"
#include "render/state_manager/render_view_state_manager.h"

namespace Mizu
{

inline constexpr size_t MAX_LAYER_OVERRIDE_COMPONENTS = std::variant_size_v<LayerComponentOverridesVariant>;

template <typename T>
concept LayerOverridableComponent = is_variant_alternative_v<T, LayerComponentOverridesVariant>;

struct RenderSettingsLayerStaticState
{
};

struct RenderSettingsLayerDynamicState
{
    uint32_t priority = 0;
    RenderViewMask render_view_mask = RENDER_VIEW_MASK_ALL;

    template <LayerOverridableComponent T>
    T& override_component()
    {
        if (T* component = get_component_opt<T>())
        {
            return *component;
        }

        return add_component_internal<T>();
    }

    template <LayerOverridableComponent T>
    void remove_component()
    {
        if (T* component = get_component_opt<T>())
        {
            remove_component_internal<T>();
        }
        else
        {
            MIZU_LOG_WARNING("Override component is not present on RenderSettingsLayer: '{}'", typeid(T).name());
        }
    }

    template <LayerOverridableComponent T>
    const T* get_component_opt() const
    {
        for (const LayerComponentOverridesVariant& component : m_components)
        {
            if (const T* value = std::get_if<T>(&component))
            {
                return value;
            }
        }

        return nullptr;
    }

    template <LayerOverridableComponent T>
    T* get_component_opt()
    {
        for (LayerComponentOverridesVariant& component : m_components)
        {
            if (T* value = std::get_if<T>(&component))
            {
                return value;
            }
        }

        return nullptr;
    }

    template <LayerOverridableComponent T>
    bool has_component()
    {
        return get_component_opt<T>() != nullptr;
    }

    std::span<const LayerComponentOverridesVariant> get_components() const { return m_components; }

    bool has_changed(const RenderSettingsLayerDynamicState& other) const
    {
        if (priority != other.priority)
        {
            return true;
        }

        if (render_view_mask != other.render_view_mask)
        {
            return true;
        }

        if (m_components.size() != other.m_components.size())
        {
            return true;
        }

        for (const LayerComponentOverridesVariant& component : m_components)
        {
            const bool component_has_changed = std::visit(
                [&](const auto& value) -> bool {
                    using T = std::decay_t<decltype(value)>;

                    const T* other_value = other.get_component_opt<T>();
                    if (other_value == nullptr)
                    {
                        return true;
                    }

                    return value != *other_value;
                },
                component);

            if (component_has_changed)
            {
                return true;
            }
        }

        return false;
    }

  private:
    inplace_vector<LayerComponentOverridesVariant, MAX_LAYER_OVERRIDE_COMPONENTS> m_components;

    template <LayerOverridableComponent T>
    T& add_component_internal()
    {
        LayerComponentOverridesVariant& component = m_components.emplace_back(T{});
        return std::get<T>(component);
    }

    template <LayerOverridableComponent T>
    void remove_component_internal()
    {
        for (auto it = m_components.begin(); it != m_components.end(); ++it)
        {
            if (std::holds_alternative<T>(*it))
            {
                m_components.erase(it, it + 1);
                return;
            }
        }
    }
};

MIZU_STATE_MANAGER_CREATE_HANDLE(RenderSettingsLayerHandle);

struct RenderSettingsLayerConfig : BaseStateManagerConfig
{
    static constexpr uint64_t MaxNumHandles = 20;

    static constexpr std::string_view Identifier = "RenderSettingsLayerStateManager";
};

using RenderSettingsLayerStateManager = BaseStateManager<
    RenderSettingsLayerStaticState,
    RenderSettingsLayerDynamicState,
    RenderSettingsLayerHandle,
    RenderSettingsLayerConfig>;
using RenderSettingsLayerStateManagerConsumer = IStateManagerConsumer<RenderSettingsLayerStateManager>;

MIZU_RENDER_API extern RenderSettingsLayerStateManager* g_render_settings_layer_state_manager;

} // namespace Mizu
