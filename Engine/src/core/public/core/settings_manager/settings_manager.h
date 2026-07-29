#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#include "base/debug/assert.h"

#include "core/settings_manager/settings.h"
#include "mizu_core_module.h"

namespace Mizu
{

class MIZU_CORE_API SettingsManager
{
  public:
    static SettingsManager& get();

    template <typename T>
    T& register_setting()
    {
        static_assert(std::is_default_constructible_v<T>, "A setting struct must be default constructible");

        SettingEntry* entry = register_setting_internal(
            std::type_index(typeid(T)),
            T::get_name(),
            T::get_members(),
            SettingStorage(
                reinterpret_cast<uint8_t*>(new T{}), [](uint8_t* ptr) { delete reinterpret_cast<T*>(ptr); }));

        if (entry == nullptr)
        {
            return get_fallback_setting<T>();
        }

        return *reinterpret_cast<T*>(entry->data.get());
    }

    template <typename T>
    bool has_setting() const
    {
        std::shared_lock lock(m_mutex);
        return find_entry(std::type_index(typeid(T))) != nullptr;
    }

    template <typename T>
    T& get_setting()
    {
        std::shared_lock lock(m_mutex);

        SettingEntry* entry = find_entry(std::type_index(typeid(T)));
        if (entry == nullptr)
        {
            MIZU_ASSERT(false, "Setting '{}' is not registered", T::get_name());
            return get_fallback_setting<T>();
        }

        return *reinterpret_cast<T*>(entry->data.get());
    }

    template <typename T>
    const T& get_setting() const
    {
        std::shared_lock lock(m_mutex);

        const SettingEntry* entry = find_entry(std::type_index(typeid(T)));
        if (entry == nullptr)
        {
            MIZU_ASSERT(false, "Setting '{}' is not registered", T::get_name());
            return get_fallback_setting<T>();
        }

        return *reinterpret_cast<const T*>(entry->data.get());
    }

    template <typename T>
    void set_setting(const T& value)
    {
        std::unique_lock lock(m_mutex);

        SettingEntry* entry = find_entry(std::type_index(typeid(T)));
        if (entry == nullptr)
        {
            MIZU_ASSERT(false, "Setting '{}' is not registered", T::get_name());
            return;
        }

        *reinterpret_cast<T*>(entry->data.get()) = value;
    }

    bool has_setting(std::string_view setting_name) const;
    bool has_member(std::string_view setting_name, std::string_view member_name) const;

    std::span<const SettingMember> get_members(std::string_view setting_name) const;

    template <typename T>
    T get_member_value(std::string_view setting_name, std::string_view member_name) const
    {
        static_assert(std::is_trivially_copyable_v<T>, "Only trivially copyable members can be accessed by name");

        T value{};
        read_member(setting_name, member_name, typeid(T), reinterpret_cast<uint8_t*>(&value), sizeof(T));
        return value;
    }

    template <typename T>
    void set_member_value(std::string_view setting_name, std::string_view member_name, const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "Only trivially copyable members can be accessed by name");

        write_member(setting_name, member_name, typeid(T), reinterpret_cast<const uint8_t*>(&value), sizeof(T));
    }

    bool set_member_value_from_string(
        std::string_view setting_name,
        std::string_view member_name,
        std::string_view value);

  private:
    using SettingStorage = std::unique_ptr<uint8_t, void (*)(uint8_t*)>;

    struct SettingEntry
    {
        std::string_view name;
        std::type_index type;
        std::span<const SettingMember> members;
        SettingStorage data;
    };

    std::unordered_map<std::type_index, std::unique_ptr<SettingEntry>> m_settings;
    std::unordered_map<std::string_view, SettingEntry*> m_settings_by_name;

    mutable std::shared_mutex m_mutex;

    template <typename T>
    static T& get_fallback_setting()
    {
        static T fallback{};
        return fallback;
    }

    SettingEntry* register_setting_internal(
        std::type_index type,
        std::string_view name,
        std::span<const SettingMember> members,
        SettingStorage&& data);

    const SettingEntry* find_entry(std::type_index type) const;
    SettingEntry* find_entry(std::type_index type);

    const SettingEntry* find_entry(std::string_view setting_name) const;
    const SettingMember* find_member(
        std::string_view setting_name,
        std::string_view member_name,
        const SettingEntry** out_entry) const;

    bool read_member(
        std::string_view setting_name,
        std::string_view member_name,
        const std::type_info& type,
        uint8_t* dst,
        size_t size) const;
    bool write_member(
        std::string_view setting_name,
        std::string_view member_name,
        const std::type_info& type,
        const uint8_t* src,
        size_t size);
};

template <typename T>
T& get_setting()
{
    return SettingsManager::get().get_setting<T>();
}

template <typename T>
struct SettingRegistrar
{
    SettingRegistrar() { SettingsManager::get().register_setting<T>(); }
};

#define MIZU_REGISTER_SETTING(_struct_name) \
    inline const Mizu::SettingRegistrar<_struct_name> g_mizu_setting_registrar_##_struct_name {}

#define MIZU_CREATE_AND_REGISTER_SETTING(_struct_name, _members) \
    MIZU_CREATE_SETTING(_struct_name, _members);                 \
    MIZU_REGISTER_SETTING(_struct_name)

} // namespace Mizu
