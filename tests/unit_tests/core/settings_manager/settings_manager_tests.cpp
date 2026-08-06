#include <catch2/catch_all.hpp>

#include <cstdint>

#include "core/settings_manager/settings_manager.h"

using namespace Mizu;

enum class TestQuality
{
    Low,
    Medium,
    High,
};

#define TEST_RENDER_SETTING_MEMBERS(X)  \
    X(int32_t, shadow_resolution, 1024) \
    X(float, exposure, 1.0f)            \
    X(bool, enable_bloom, true)         \
    X(TestQuality, quality, TestQuality::Medium)

MIZU_CREATE_AND_REGISTER_SETTING(TestRenderSetting, TEST_RENDER_SETTING_MEMBERS);

#define TEST_AUDIO_SETTING_MEMBERS(X) \
    X(float, master_volume, 0.5f)     \
    X(uint32_t, num_channels, 2)

MIZU_CREATE_SETTING(TestAudioSetting, TEST_AUDIO_SETTING_MEMBERS);

TEST_CASE("SettingsManager registers settings declared with MIZU_CREATE_AND_REGISTER_SETTING", "[settings_manager]")
{
    const SettingsManager& settings = SettingsManager::get();

    REQUIRE(settings.has_setting<TestRenderSetting>());
    REQUIRE(settings.has_setting("TestRenderSetting"));
}

TEST_CASE("SettingsManager registers settings manually", "[settings_manager]")
{
    SettingsManager& settings = SettingsManager::get();

    settings.register_setting<TestAudioSetting>();

    REQUIRE(settings.has_setting<TestAudioSetting>());
    REQUIRE(settings.has_setting("TestAudioSetting"));

    settings.set_member_value<uint32_t>("TestAudioSetting", "num_channels", 6);

    settings.register_setting<TestAudioSetting>();
    REQUIRE(settings.get_setting<TestAudioSetting>().num_channels == 6);

    settings.set_setting(TestAudioSetting{});
}

TEST_CASE("SettingsManager settings are created with their default values", "[settings_manager]")
{
    const SettingsManager& settings = SettingsManager::get();

    const TestRenderSetting& setting = settings.get_setting<TestRenderSetting>();

    REQUIRE(setting.shadow_resolution == 1024);
    REQUIRE(setting.exposure == Catch::Approx(1.0f));
    REQUIRE(setting.enable_bloom);
    REQUIRE(setting.quality == TestQuality::Medium);
}

TEST_CASE("SettingsManager accesses settings by type", "[settings_manager]")
{
    SettingsManager& settings = SettingsManager::get();

    TestRenderSetting& setting = settings.get_setting<TestRenderSetting>();
    setting.shadow_resolution = 4096;
    setting.enable_bloom = false;

    REQUIRE(settings.get_setting<TestRenderSetting>().shadow_resolution == 4096);
    REQUIRE_FALSE(settings.get_setting<TestRenderSetting>().enable_bloom);

    settings.set_setting(TestRenderSetting{});

    REQUIRE(settings.get_setting<TestRenderSetting>().shadow_resolution == 1024);
    REQUIRE(settings.get_setting<TestRenderSetting>().enable_bloom);
}

TEST_CASE("SettingsManager accesses members by name", "[settings_manager]")
{
    SettingsManager& settings = SettingsManager::get();

    REQUIRE(settings.get_member_value<int32_t>("TestRenderSetting", "shadow_resolution") == 1024);
    REQUIRE(settings.get_member_value<float>("TestRenderSetting", "exposure") == Catch::Approx(1.0f));
    REQUIRE(settings.get_member_value<bool>("TestRenderSetting", "enable_bloom"));

    settings.set_member_value<int32_t>("TestRenderSetting", "shadow_resolution", 2048);
    settings.set_member_value<float>("TestRenderSetting", "exposure", 2.5f);

    const TestRenderSetting& setting = settings.get_setting<TestRenderSetting>();
    REQUIRE(setting.shadow_resolution == 2048);
    REQUIRE(setting.exposure == Catch::Approx(2.5f));
    REQUIRE(setting.enable_bloom);

    settings.set_setting(TestRenderSetting{});
}

TEST_CASE("SettingsManager exposes the members of a setting", "[settings_manager]")
{
    const SettingsManager& settings = SettingsManager::get();

    const std::span<const SettingMember> members = settings.get_members("TestRenderSetting");
    REQUIRE(members.size() == 4);

    REQUIRE(members[0].name == "shadow_resolution");
    REQUIRE(members[0].type == typeid(int32_t));
    REQUIRE(members[0].size == sizeof(int32_t));
    REQUIRE(members[0].offset == offsetof(TestRenderSetting, shadow_resolution));

    REQUIRE(members[1].name == "exposure");
    REQUIRE(members[1].type == typeid(float));

    REQUIRE(members[2].name == "enable_bloom");
    REQUIRE(members[2].type == typeid(bool));

    REQUIRE(members[3].name == "quality");
    REQUIRE(members[3].type == typeid(TestQuality));
}

TEST_CASE("SettingsManager sets members from a string", "[settings_manager]")
{
    SettingsManager& settings = SettingsManager::get();

    REQUIRE(settings.set_member_value_from_string("TestRenderSetting", "quality", "High"));
    REQUIRE(settings.set_member_value_from_string("TestRenderSetting", "shadow_resolution", "2048"));
    REQUIRE(settings.set_member_value_from_string("TestRenderSetting", "exposure", "2.5"));
    REQUIRE(settings.set_member_value_from_string("TestRenderSetting", "enable_bloom", "false"));

    const TestRenderSetting& setting = settings.get_setting<TestRenderSetting>();
    REQUIRE(setting.quality == TestQuality::High);
    REQUIRE(setting.shadow_resolution == 2048);
    REQUIRE(setting.exposure == Catch::Approx(2.5f));
    REQUIRE_FALSE(setting.enable_bloom);

    settings.set_setting(TestRenderSetting{});
}

TEST_CASE("SettingsManager rejects strings that don't match the member type", "[settings_manager]")
{
    SettingsManager& settings = SettingsManager::get();

    REQUIRE_FALSE(settings.set_member_value_from_string("TestRenderSetting", "quality", "Ultra"));
    REQUIRE_FALSE(settings.set_member_value_from_string("TestRenderSetting", "shadow_resolution", "1024a"));
    REQUIRE_FALSE(settings.set_member_value_from_string("TestRenderSetting", "enable_bloom", "yes"));
    REQUIRE_FALSE(settings.set_member_value_from_string("TestRenderSetting", "does_not_exist", "1"));
    REQUIRE_FALSE(settings.set_member_value_from_string("DoesNotExist", "quality", "High"));

    // A failed parse must leave the member untouched
    const TestRenderSetting& setting = settings.get_setting<TestRenderSetting>();
    REQUIRE(setting.quality == TestQuality::Medium);
    REQUIRE(setting.shadow_resolution == 1024);
    REQUIRE(setting.enable_bloom);
}

TEST_CASE("SettingsManager reports unknown settings and members", "[settings_manager]")
{
    const SettingsManager& settings = SettingsManager::get();

    REQUIRE_FALSE(settings.has_setting("DoesNotExist"));

    REQUIRE(settings.has_member("TestRenderSetting", "exposure"));
    REQUIRE_FALSE(settings.has_member("TestRenderSetting", "does_not_exist"));
    REQUIRE_FALSE(settings.has_member("DoesNotExist", "exposure"));
}
