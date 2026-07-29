#include "core/settings_manager/settings_manager.h"

#include <cstring>
#include <string>

#include "base/debug/logging.h"

namespace Mizu
{

SettingsManager& SettingsManager::get()
{
    static SettingsManager instance{};
    return instance;
}

SettingsManager::SettingEntry* SettingsManager::register_setting_internal(
    std::type_index type,
    std::string_view name,
    std::span<const SettingMember> members,
    SettingStorage&& data)
{
    std::unique_lock lock(m_mutex);

    if (SettingEntry* registered = find_entry(type); registered != nullptr)
    {
        return registered;
    }

    if (const auto it = m_settings_by_name.find(name); it != m_settings_by_name.end())
    {
        MIZU_ASSERT(
            false, "Setting name '{}' is already registered by a different type ('{}')", name, it->second->type.name());
        return nullptr;
    }

    auto entry = std::make_unique<SettingEntry>(SettingEntry{
        .name = name,
        .type = type,
        .members = members,
        .data = std::move(data),
    });

    SettingEntry* entry_ptr = entry.get();

    m_settings.insert({type, std::move(entry)});
    m_settings_by_name.insert({name, entry_ptr});

    return entry_ptr;
}

bool SettingsManager::has_setting(std::string_view setting_name) const
{
    std::shared_lock lock(m_mutex);
    return find_entry(setting_name) != nullptr;
}

bool SettingsManager::has_member(std::string_view setting_name, std::string_view member_name) const
{
    std::shared_lock lock(m_mutex);

    const SettingEntry* entry = nullptr;
    return find_member(setting_name, member_name, &entry) != nullptr;
}

std::span<const SettingMember> SettingsManager::get_members(std::string_view setting_name) const
{
    std::shared_lock lock(m_mutex);

    const SettingEntry* entry = find_entry(setting_name);
    if (entry == nullptr)
    {
        MIZU_ASSERT(false, "Setting '{}' is not registered", setting_name);
        return {};
    }

    return entry->members;
}

bool SettingsManager::set_member_value_from_string(
    std::string_view setting_name,
    std::string_view member_name,
    std::string_view value)
{
    std::unique_lock lock(m_mutex);

    const SettingEntry* entry = nullptr;
    const SettingMember* member = find_member(setting_name, member_name, &entry);
    if (member == nullptr)
    {
        MIZU_LOG_ERROR("Setting '{}' does not have a member named '{}'", setting_name, member_name);
        return false;
    }

    if (!member->type_reflection.parse(value, entry->data.get() + member->offset))
    {
        const std::span<const std::string_view> allowed = member->type_reflection.allowed_values();
        if (allowed.empty())
        {
            MIZU_LOG_ERROR(
                "Can't parse '{}' as a value of member '{}.{}', which is a '{}'",
                value,
                setting_name,
                member_name,
                member->type.name());
        }
        else
        {
            std::string allowed_str{};
            for (const std::string_view allowed_value : allowed)
            {
                if (!allowed_str.empty())
                    allowed_str += ", ";

                allowed_str += allowed_value;
            }

            MIZU_LOG_ERROR(
                "'{}' is not a valid value of member '{}.{}', expected one of: {}",
                value,
                setting_name,
                member_name,
                allowed_str);
        }

        return false;
    }

    return true;
}

const SettingsManager::SettingEntry* SettingsManager::find_entry(std::type_index type) const
{
    const auto it = m_settings.find(type);
    return it != m_settings.end() ? it->second.get() : nullptr;
}

SettingsManager::SettingEntry* SettingsManager::find_entry(std::type_index type)
{
    const auto it = m_settings.find(type);
    return it != m_settings.end() ? it->second.get() : nullptr;
}

const SettingsManager::SettingEntry* SettingsManager::find_entry(std::string_view setting_name) const
{
    const auto it = m_settings_by_name.find(setting_name);
    return it != m_settings_by_name.end() ? it->second : nullptr;
}

const SettingMember* SettingsManager::find_member(
    std::string_view setting_name,
    std::string_view member_name,
    const SettingEntry** out_entry) const
{
    const SettingEntry* entry = find_entry(setting_name);
    if (entry == nullptr)
    {
        return nullptr;
    }

    for (const SettingMember& member : entry->members)
    {
        if (member.name == member_name)
        {
            *out_entry = entry;
            return &member;
        }
    }

    return nullptr;
}

bool SettingsManager::read_member(
    std::string_view setting_name,
    std::string_view member_name,
    const std::type_info& type,
    uint8_t* dst,
    size_t size) const
{
    std::shared_lock lock(m_mutex);

    const SettingEntry* entry = nullptr;
    const SettingMember* member = find_member(setting_name, member_name, &entry);
    if (member == nullptr)
    {
        MIZU_ASSERT(false, "Setting '{}' does not have a member named '{}'", setting_name, member_name);
        return false;
    }

    if (member->type != type || member->size != size)
    {
        MIZU_ASSERT(
            false,
            "Type mismatch reading member '{}.{}', it is a '{}' but a '{}' was requested",
            setting_name,
            member_name,
            member->type.name(),
            type.name());
        return false;
    }

    std::memcpy(dst, entry->data.get() + member->offset, size);
    return true;
}

bool SettingsManager::write_member(
    std::string_view setting_name,
    std::string_view member_name,
    const std::type_info& type,
    const uint8_t* src,
    size_t size)
{
    std::unique_lock lock(m_mutex);

    const SettingEntry* entry = nullptr;
    const SettingMember* member = find_member(setting_name, member_name, &entry);
    if (member == nullptr)
    {
        MIZU_ASSERT(false, "Setting '{}' does not have a member named '{}'", setting_name, member_name);
        return false;
    }

    if (member->type != type || member->size != size)
    {
        MIZU_ASSERT(
            false,
            "Type mismatch writing member '{}.{}', it is a '{}' but a '{}' was provided",
            setting_name,
            member_name,
            member->type.name(),
            type.name());
        return false;
    }

    std::memcpy(entry->data.get() + member->offset, src, size);
    return true;
}

} // namespace Mizu
