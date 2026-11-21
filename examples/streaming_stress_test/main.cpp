#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <limits>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

#include "asset/asset_registry.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "camera_controllers/editor_camera_controller.h"
#include "core/game_context.h"
#include "core/input.h"
#include "core/settings_manager/settings_manager.h"
#include "core/window.h"
#include "render/core/camera.h"
#include "render/state_manager/light_state_manager.h"
#include "render/state_manager/render_view_state_manager.h"
#include "render/state_manager/transform_state_manager.h"
#include "render/streaming/streaming_settings.h"
#include "runtime/game_main.h"
#include "runtime/main_loop.h"
#include "world/prefab/prefab_manager.h"

using namespace Mizu;

namespace
{

constexpr uint32_t SCENARIO_SEED = 0x5EED;

constexpr double NEVER_TIME = std::numeric_limits<double>::infinity();
constexpr uint64_t NEVER_FRAME = std::numeric_limits<uint64_t>::max();

const glm::vec3 SPAWN_VOLUME_MIN{-60.0f, 0.5f, -12.0f};
const glm::vec3 SPAWN_VOLUME_MAX{60.0f, 15.0f, 12.0f};

constexpr float FAR_SPAWN_CHANCE = 0.15f;
constexpr float FAR_SPAWN_MIN_RADIUS = 80.0f;
constexpr float FAR_SPAWN_MAX_RADIUS = 250.0f;

constexpr float CAMERA_ZNEAR = 0.1f;
constexpr float CAMERA_ZFAR = 600.0f;
constexpr float DEFAULT_FOV = glm::radians(60.0f);

constexpr float DOLLY_MIN_DISTANCE = 1.0f;
constexpr float DOLLY_MAX_DISTANCE = 150.0f;
constexpr float DOLLY_FREQUENCY = 1.0f;
constexpr float ORBIT_ANGULAR_SPEED = 2.0f;
constexpr float FOV_ZOOM_MIN = glm::radians(10.0f);
constexpr float FOV_ZOOM_MAX = glm::radians(100.0f);
constexpr float FOV_ZOOM_FREQUENCY = 0.5f;

constexpr float PIP_VIEW_SIZE = 0.25f;
constexpr float TINY_VIEW_SIZE = 0.08f;
constexpr float VIEW_MARGIN = 0.02f;
constexpr float MIN_VIEW_LIFETIME = 1.0f;
constexpr float MAX_VIEW_LIFETIME = 4.0f;

constexpr double STATS_LOG_INTERVAL = 1.0;

const std::string_view SPONZA_PATH = "shared:Models/Sponza/glTF/Sponza.gltf";
constexpr float SPONZA_SCALE = 0.05f;

constexpr double SPONZA_FLICKER_FIRST_TIME = 30.0;
constexpr double SPONZA_FLICKER_INTERVAL = 20.0;
constexpr float SPONZA_FLICKER_EVICT_WAIT = 3.0f;

struct StressAsset
{
    std::string_view path;
    glm::vec3 base_rotation;
    float min_scale;
    float max_scale;
    float spawn_weight;
};

constexpr size_t NUM_SPAWNABLE_ASSETS = 3;

const std::array<StressAsset, NUM_SPAWNABLE_ASSETS> SPAWNABLE_ASSETS = {{
    {
        .path = "shared:Models/FlightHelmet/glTF/FlightHelmet.gltf",
        .base_rotation = glm::vec3(0.0f),
        .min_scale = 1.0f,
        .max_scale = 6.0f,
        .spawn_weight = 1.0f,
    },
    {
        .path = "shared:Models/DamagedHelmet/glTF/DamagedHelmet.gltf",
        .base_rotation = glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f),
        .min_scale = 0.3f,
        .max_scale = 2.5f,
        .spawn_weight = 3.0f,
    },
    {
        .path = "shared:Models/Avocado/glTF/Avocado.gltf",
        .base_rotation = glm::vec3(0.0f, glm::pi<float>(), 0.0f),
        .min_scale = 8.0f,
        .max_scale = 60.0f,
        .spawn_weight = 1.5f,
    },
}};

struct StressConfig
{
    std::string_view name;

    uint32_t max_instances;
    float spawn_rate;
    float flicker_chance;
    float drift_chance;
    float min_lifetime;
    float max_lifetime;

    float wave_interval;
    uint32_t wave_burst;
    float sponza_rebuild_chance;
    float recycle_interval;

    float camera_pattern_duration;
    float teleport_interval;
    float whip_interval;

    float min_view_interval;
    float max_view_interval;
    float view_blink_chance;

    float mode_interval;
    float mode_hold;
};

const StressConfig SOAK_CONFIG{
    .name = "soak",
    .max_instances = 60,
    .spawn_rate = 4.0f,
    .flicker_chance = 0.02f,
    .drift_chance = 0.1f,
    .min_lifetime = 4.0f,
    .max_lifetime = 15.0f,
    .wave_interval = 45.0f,
    .wave_burst = 30,
    .sponza_rebuild_chance = 0.1f,
    .recycle_interval = 20.0f,
    .camera_pattern_duration = 12.0f,
    .teleport_interval = 1.5f,
    .whip_interval = 2.0f,
    .min_view_interval = 10.0f,
    .max_view_interval = 20.0f,
    .view_blink_chance = 0.05f,
    .mode_interval = 60.0f,
    .mode_hold = 5.0f,
};

const StressConfig CHAOS_CONFIG{
    .name = "chaos",
    .max_instances = 200,
    .spawn_rate = 20.0f,
    .flicker_chance = 0.1f,
    .drift_chance = 0.3f,
    .min_lifetime = 0.5f,
    .max_lifetime = 5.0f,
    .wave_interval = 15.0f,
    .wave_burst = 100,
    .sponza_rebuild_chance = 0.3f,
    .recycle_interval = 4.0f,
    .camera_pattern_duration = 6.0f,
    .teleport_interval = 0.3f,
    .whip_interval = 0.5f,
    .min_view_interval = 5.0f,
    .max_view_interval = 10.0f,
    .view_blink_chance = 0.2f,
    .mode_interval = 20.0f,
    .mode_hold = 3.0f,
};

enum class CameraPattern
{
    Dolly,
    Teleport,
    WhipTurn,
    Orbit,
    FovZoom,
    Count,
};

enum class ViewPreset
{
    CloseUp,
    Opposite,
    Tiny,
    SplitScreen,
    Count,
};

std::string_view to_string(CameraPattern pattern)
{
    switch (pattern)
    {
    case CameraPattern::Dolly:
        return "dolly";
    case CameraPattern::Teleport:
        return "teleport";
    case CameraPattern::WhipTurn:
        return "whip_turn";
    case CameraPattern::Orbit:
        return "orbit";
    case CameraPattern::FovZoom:
        return "fov_zoom";
    case CameraPattern::Count:
        break;
    }

    return "unknown";
}

std::string_view to_string(ViewPreset preset)
{
    switch (preset)
    {
    case ViewPreset::CloseUp:
        return "close_up";
    case ViewPreset::Opposite:
        return "opposite";
    case ViewPreset::Tiny:
        return "tiny";
    case ViewPreset::SplitScreen:
        return "split_screen";
    case ViewPreset::Count:
        break;
    }

    return "unknown";
}

std::string_view to_string(StreamingMode mode)
{
    switch (mode)
    {
    case StreamingMode::Enabled:
        return "enabled";
    case StreamingMode::FullResidency:
        return "full_residency";
    case StreamingMode::Frozen:
        return "frozen";
    }

    return "unknown";
}

glm::vec3 direction_from_angles(float pitch, float yaw)
{
    return glm::vec3(std::cos(pitch) * std::sin(yaw), -std::sin(pitch), -std::cos(pitch) * std::cos(yaw));
}

glm::vec3 look_rotation(const glm::vec3& direction)
{
    const float length = glm::length(direction);
    if (length < 0.0001f)
        return glm::vec3(0.0f);

    const glm::vec3 dir = direction / length;
    return glm::vec3(std::asin(glm::clamp(-dir.y, -1.0f, 1.0f)), std::atan2(dir.x, -dir.z), 0.0f);
}

float oscillate(float time, float frequency)
{
    return 0.5f - 0.5f * std::cos(time * frequency * glm::two_pi<float>());
}

class KeyToggle
{
  public:
    explicit KeyToggle(Key key) : m_key(key) {}

    bool consume_press()
    {
        const bool is_down = Input::is_key_pressed(m_key);
        const bool pressed = is_down && !m_was_down;
        m_was_down = is_down;

        return pressed;
    }

  private:
    Key m_key;
    bool m_was_down = false;
};

struct StressInstance
{
    PrefabController controller{};
    uint32_t asset_index = 0;

    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    bool drifting = false;
    float spin_speed = 0.0f;

    double despawn_time = NEVER_TIME;
    uint64_t despawn_frame = NEVER_FRAME;
};

struct ExtraView
{
    RenderViewHandle handle{};
    ViewPreset preset = ViewPreset::CloseUp;
    Camera camera{};

    double close_time = NEVER_TIME;
    uint64_t close_frame = NEVER_FRAME;
};

struct StressStats
{
    uint64_t spawned = 0;
    uint64_t despawned = 0;
    uint64_t flickers = 0;
    uint64_t waves = 0;
    uint64_t recycles = 0;
    uint64_t sponza_rebuilds = 0;
    uint64_t views_opened = 0;
    uint64_t mode_switches = 0;
};

} // namespace

class StreamingStressTestSimulation : public GameSimulation
{
  public:
    void init() override
    {
        MIZU_PROFILE_SCOPED;

        const uint32_t width = g_game_context->get_window().get_width();
        const uint32_t height = g_game_context->get_window().get_height();
        m_aspect_ratio = static_cast<float>(width) / static_cast<float>(height);

        m_free_camera_controller =
            std::make_unique<EditorCameraController>(DEFAULT_FOV, m_aspect_ratio, CAMERA_ZNEAR, CAMERA_ZFAR);

        MIZU_LOG_INFO(
            "[StreamingStressTest] seed={} preset={} | keys: F free camera, K pause, M cycle streaming mode, "
            "N toggle preset, L toggle streaming logging",
            SCENARIO_SEED,
            m_config->name);

        AssetRegistry& asset_registry = g_game_context->get_asset_registry();

        m_sponza_handle = asset_registry.get_prefab_handle(SPONZA_PATH);

        std::vector<float> spawn_weights;
        for (size_t i = 0; i < SPAWNABLE_ASSETS.size(); ++i)
        {
            m_asset_handles[i] = asset_registry.get_prefab_handle(SPAWNABLE_ASSETS[i].path);
            spawn_weights.push_back(SPAWNABLE_ASSETS[i].spawn_weight);
        }
        m_asset_distribution = std::discrete_distribution<uint32_t>(spawn_weights.begin(), spawn_weights.end());

        spawn_sponza();
        create_sun();

        RenderViewDynamicState main_view_ds{};
        main_view_ds.viewport = ViewportRect{};
        main_view_ds.layer = 0;
        main_view_ds.camera = m_free_camera_controller->get_camera();

        m_main_view_handle = g_render_view_state_manager->sim_create(RenderViewStaticState{}, main_view_ds);

        m_scripted_camera = m_free_camera_controller->get_camera();

        reset_schedule();
        start_camera_pattern(CameraPattern::Dolly);

        if (const char* drain_after = std::getenv("MIZU_STRESS_DRAIN_AFTER"))
        {
            m_drain_time = std::strtod(drain_after, nullptr);
            MIZU_LOG_INFO("[StreamingStressTest] drain scheduled at t={:.1f}s", m_drain_time);
        }

        m_next_sponza_flicker_time = SPONZA_FLICKER_FIRST_TIME;
    }

    void update(double dt) override
    {
        MIZU_PROFILE_SCOPED;

        m_time += dt;
        ++m_frame;

        handle_input();

        if (m_time >= m_drain_time)
        {
            drain();
            m_drain_time = NEVER_TIME;
        }

        if (!m_paused)
        {
            update_spawner(dt);
            update_events();
            update_instances(dt);
            update_views();
            update_streaming_mode();
            update_scripted_camera();
        }

        if (m_free_camera)
        {
            m_free_camera_controller->update(dt);
        }

        update_render_views();
        log_stats();
    }

    void on_window_resized(WindowResizedEvent& event) override
    {
        if (event.get_width() == 0)
            return;

        if (event.get_height() == 0)
            return;

        m_aspect_ratio = static_cast<float>(event.get_width()) / static_cast<float>(event.get_height());
        m_free_camera_controller->set_aspect_ratio(m_aspect_ratio);
    }

  private:
    const StressConfig* m_config = &CHAOS_CONFIG;

    std::mt19937 m_rng{SCENARIO_SEED};
    std::discrete_distribution<uint32_t> m_asset_distribution{};

    double m_time = 0.0;
    uint64_t m_frame = 0;
    bool m_paused = false;
    double m_drain_time = NEVER_TIME;

    PrefabManager m_prefab_manager;

    PrefabAssetHandle m_sponza_handle{};
    PrefabController m_sponza{};
    double m_sponza_respawn_time = NEVER_TIME;

    double m_next_sponza_flicker_time = NEVER_TIME;
    bool m_sponza_flicker_armed = false;
    uint64_t m_sponza_flicker_destroy_frame = NEVER_FRAME;

    std::array<PrefabAssetHandle, NUM_SPAWNABLE_ASSETS> m_asset_handles{};
    std::vector<StressInstance> m_instances;
    double m_spawn_accumulator = 0.0;

    double m_next_wave_time = 0.0;
    double m_next_recycle_time = 0.0;

    LightHandle m_sun_handle{};

    float m_aspect_ratio = 1.0f;
    bool m_free_camera = false;
    std::unique_ptr<EditorCameraController> m_free_camera_controller;

    Camera m_scripted_camera{};
    CameraPattern m_camera_pattern = CameraPattern::Dolly;
    double m_camera_pattern_start_time = 0.0;
    double m_next_camera_event_time = 0.0;
    glm::vec3 m_camera_focus{0.0f};
    glm::vec3 m_camera_anchor{0.0f};
    glm::vec3 m_camera_direction{0.0f, 0.0f, -1.0f};
    float m_whip_yaw = 0.0f;
    float m_whip_pitch = 0.0f;

    RenderViewHandle m_main_view_handle{};
    std::vector<ExtraView> m_extra_views;
    bool m_main_view_split = false;
    double m_next_view_time = 0.0;

    double m_next_mode_time = 0.0;
    double m_mode_restore_time = NEVER_TIME;

    StressStats m_stats{};
    double m_next_stats_log_time = 0.0;

    KeyToggle m_free_camera_key{Key::F};
    KeyToggle m_pause_key{Key::K};
    KeyToggle m_mode_key{Key::M};
    KeyToggle m_preset_key{Key::N};
    KeyToggle m_logging_key{Key::L};

    //
    // Random helpers
    //

    float random_float(float min, float max)
    {
        std::uniform_real_distribution<float> distribution(min, max);
        return distribution(m_rng);
    }

    uint32_t random_uint(uint32_t min, uint32_t max)
    {
        std::uniform_int_distribution<uint32_t> distribution(min, max);
        return distribution(m_rng);
    }

    bool chance(float probability) { return random_float(0.0f, 1.0f) < probability; }

    glm::vec3 random_in_box(const glm::vec3& min, const glm::vec3& max)
    {
        return glm::vec3(random_float(min.x, max.x), random_float(min.y, max.y), random_float(min.z, max.z));
    }

    glm::vec3 random_direction()
    {
        return direction_from_angles(random_float(-0.4f, 0.4f), random_float(0.0f, glm::two_pi<float>()));
    }

    glm::vec3 random_spawn_position()
    {
        if (chance(FAR_SPAWN_CHANCE))
        {
            const float angle = random_float(0.0f, glm::two_pi<float>());
            const float radius = random_float(FAR_SPAWN_MIN_RADIUS, FAR_SPAWN_MAX_RADIUS);
            return glm::vec3(std::cos(angle) * radius, random_float(0.0f, 30.0f), std::sin(angle) * radius);
        }

        return random_in_box(SPAWN_VOLUME_MIN, SPAWN_VOLUME_MAX);
    }

    glm::vec3 random_focus_point()
    {
        if (!m_instances.empty())
        {
            if (chance(0.75f))
            {
                const uint32_t index = random_uint(0, static_cast<uint32_t>(m_instances.size() - 1));
                return m_instances[index].position;
            }
        }

        return random_in_box(SPAWN_VOLUME_MIN, SPAWN_VOLUME_MAX);
    }

    //
    // Input
    //

    void handle_input()
    {
        if (m_free_camera_key.consume_press())
        {
            m_free_camera = !m_free_camera;

            if (m_free_camera)
            {
                m_free_camera_controller->set_position(m_scripted_camera.position);
                m_free_camera_controller->set_rotation(m_scripted_camera.rotation);
                m_free_camera_controller->set_fov(DEFAULT_FOV);
            }

            MIZU_LOG_INFO("[StreamingStressTest] free camera: {}", m_free_camera);
        }

        if (m_pause_key.consume_press())
        {
            m_paused = !m_paused;
            MIZU_LOG_INFO("[StreamingStressTest] paused: {}", m_paused);
        }

        if (m_mode_key.consume_press())
        {
            const StreamingMode current = get_setting<StreamingSettings>().mode;
            const StreamingMode next = current == StreamingMode::Enabled         ? StreamingMode::FullResidency
                                       : current == StreamingMode::FullResidency ? StreamingMode::Frozen
                                                                                 : StreamingMode::Enabled;
            set_streaming_mode(next);

            m_mode_restore_time = NEVER_TIME;
            m_next_mode_time = m_time + m_config->mode_interval;
        }

        if (m_preset_key.consume_press())
        {
            m_config = m_config == &CHAOS_CONFIG ? &SOAK_CONFIG : &CHAOS_CONFIG;
            reset_schedule();

            MIZU_LOG_INFO("[StreamingStressTest] preset: {}", m_config->name);
        }

        if (m_logging_key.consume_press())
        {
            StreamingSettings& settings = get_setting<StreamingSettings>();
            settings.logging_enabled = !settings.logging_enabled;

            MIZU_LOG_INFO("[StreamingStressTest] streaming logging: {}", settings.logging_enabled);
        }
    }

    void reset_schedule()
    {
        m_next_wave_time = m_time + m_config->wave_interval;
        m_next_recycle_time = m_time + m_config->recycle_interval;
        m_next_view_time = m_time + random_float(m_config->min_view_interval, m_config->max_view_interval);
        m_next_mode_time = m_time + m_config->mode_interval;
    }

    //
    // Scene
    //

    void spawn_sponza()
    {
        m_sponza = m_prefab_manager.create(m_sponza_handle, TransformDynamicState{.scale = glm::vec3(SPONZA_SCALE)});
    }

    void destroy_sponza()
    {
        if (!m_sponza.is_valid())
            return;

        m_prefab_manager.destroy(m_sponza);
        m_sponza = PrefabController{};
    }

    void create_sun()
    {
        const TransformHandle transform_handle = g_transform_state_manager->sim_create({}, {});

        LightStaticState static_state{};
        static_state.transform_handle = transform_handle;
        static_state.type = LightType::Directional;

        LightDynamicState dynamic_state{};
        dynamic_state.color = glm::vec3(1.0f, 1.0f, 1.0f);
        dynamic_state.intensity = 1.0f;
        dynamic_state.cast_shadows = true;
        dynamic_state.data.directional = LightDynamicState::Directional{.direction = glm::vec3(1.0f, -1.0f, 0.0f)};

        m_sun_handle = g_light_state_manager->sim_create(static_state, dynamic_state);
    }

    //
    // Spawner
    //

    void spawn_instance(uint32_t asset_index, const glm::vec3& position, bool allow_flicker)
    {
        const StressAsset& asset = SPAWNABLE_ASSETS[asset_index];

        TransformDynamicState transform_ds{};
        transform_ds.translation = position;
        transform_ds.rotation = asset.base_rotation + glm::vec3(0.0f, random_float(0.0f, glm::two_pi<float>()), 0.0f);
        transform_ds.scale = glm::vec3(random_float(asset.min_scale, asset.max_scale));

        PrefabController controller = m_prefab_manager.create(m_asset_handles[asset_index], transform_ds);
        if (!controller.is_valid())
            return;

        StressInstance instance{};
        instance.controller = controller;
        instance.asset_index = asset_index;
        instance.position = position;
        instance.spin_speed = random_float(-2.0f, 2.0f);

        const bool above_min = glm::all(glm::greaterThanEqual(position, SPAWN_VOLUME_MIN));
        const bool below_max = glm::all(glm::lessThanEqual(position, SPAWN_VOLUME_MAX));
        if (above_min && below_max)
        {
            if (chance(m_config->drift_chance))
            {
                instance.drifting = true;
                instance.velocity = random_direction() * random_float(1.0f, 8.0f);
            }
        }

        bool flicker = false;
        if (allow_flicker)
        {
            flicker = chance(m_config->flicker_chance);
        }

        if (flicker)
        {
            instance.despawn_frame = m_frame + random_uint(1, 3);
            ++m_stats.flickers;
        }
        else
        {
            instance.despawn_time = m_time + random_float(m_config->min_lifetime, m_config->max_lifetime);
        }

        m_instances.push_back(instance);
        ++m_stats.spawned;
    }

    void spawn_random_instance(bool allow_flicker)
    {
        const uint32_t asset_index = m_asset_distribution(m_rng);
        spawn_instance(asset_index, random_spawn_position(), allow_flicker);
    }

    void destroy_instance(size_t index)
    {
        m_prefab_manager.destroy(m_instances[index].controller);

        std::swap(m_instances[index], m_instances.back());
        m_instances.pop_back();

        ++m_stats.despawned;
    }

    void destroy_all_instances()
    {
        while (!m_instances.empty())
        {
            destroy_instance(m_instances.size() - 1);
        }
    }

    void update_spawner(double dt)
    {
        MIZU_PROFILE_SCOPED;

        for (size_t i = m_instances.size(); i > 0; --i)
        {
            const StressInstance& instance = m_instances[i - 1];

            const bool expired = m_time >= instance.despawn_time || m_frame >= instance.despawn_frame;
            if (expired)
            {
                destroy_instance(i - 1);
            }
        }

        m_spawn_accumulator += dt * m_config->spawn_rate;

        while (m_spawn_accumulator >= 1.0)
        {
            m_spawn_accumulator -= 1.0;

            if (m_instances.size() >= m_config->max_instances)
            {
                m_spawn_accumulator = 0.0;
                break;
            }

            spawn_random_instance(true);
        }
    }

    void update_instances(double dt)
    {
        MIZU_PROFILE_SCOPED;

        const float fdt = static_cast<float>(dt);

        for (StressInstance& instance : m_instances)
        {
            if (instance.drifting)
            {
                instance.position += instance.velocity * fdt;

                for (int axis = 0; axis < 3; ++axis)
                {
                    const bool outside = instance.position[axis] < SPAWN_VOLUME_MIN[axis]
                                         || instance.position[axis] > SPAWN_VOLUME_MAX[axis];
                    if (outside)
                    {
                        instance.velocity[axis] = -instance.velocity[axis];
                        instance.position[axis] =
                            glm::clamp(instance.position[axis], SPAWN_VOLUME_MIN[axis], SPAWN_VOLUME_MAX[axis]);
                    }
                }
            }

            TransformDynamicState& transform_ds = instance.controller.edit_transform_ds();
            transform_ds.translation = instance.position;
            transform_ds.rotation.y += instance.spin_speed * fdt;
        }
    }

    //
    // Events
    //

    void update_events()
    {
        MIZU_PROFILE_SCOPED;

        if (m_time >= m_next_wave_time)
        {
            run_wave();
            m_next_wave_time = m_time + m_config->wave_interval;
        }

        if (m_time >= m_next_recycle_time)
        {
            run_recycle();
            m_next_recycle_time = m_time + m_config->recycle_interval;
        }

        if (m_time >= m_next_sponza_flicker_time)
        {
            destroy_sponza();
            m_sponza_respawn_time = m_time + SPONZA_FLICKER_EVICT_WAIT;
            m_sponza_flicker_armed = true;
            m_next_sponza_flicker_time = m_time + SPONZA_FLICKER_INTERVAL;
        }

        if (m_time >= m_sponza_respawn_time)
        {
            if (!m_sponza.is_valid())
            {
                spawn_sponza();
            }
            m_sponza_respawn_time = NEVER_TIME;

            if (m_sponza_flicker_armed)
            {
                m_sponza_flicker_armed = false;
                m_sponza_flicker_destroy_frame = m_frame + random_uint(5, 60);
            }
        }

        if (m_frame >= m_sponza_flicker_destroy_frame)
        {
            destroy_sponza();
            m_sponza_flicker_destroy_frame = NEVER_FRAME;
            m_sponza_respawn_time = m_time + random_float(6.0f, 8.0f);

            MIZU_LOG_INFO("[StreamingStressTest] sponza flicker: destroyed while loading, respawn in 6-8s");
        }
    }

    void run_wave()
    {
        const size_t destroyed = m_instances.size();
        destroy_all_instances();

        bool rebuild_sponza = false;
        if (chance(m_config->sponza_rebuild_chance))
        {
            if (m_sponza.is_valid())
            {
                destroy_sponza();
                rebuild_sponza = true;

                const bool same_frame = chance(0.5f);
                m_sponza_respawn_time = same_frame ? m_time : m_time + random_float(0.5f, 2.0f);
                ++m_stats.sponza_rebuilds;
            }
        }

        const uint32_t burst = std::min(m_config->wave_burst, m_config->max_instances);
        for (uint32_t i = 0; i < burst; ++i)
        {
            spawn_random_instance(false);
        }

        ++m_stats.waves;

        MIZU_LOG_INFO(
            "[StreamingStressTest] wave: destroyed={} spawned={} sponza_rebuild={}", destroyed, burst, rebuild_sponza);
    }

    void drain()
    {
        const size_t destroyed = m_instances.size();
        destroy_all_instances();
        destroy_sponza();
        m_sponza_respawn_time = NEVER_TIME;

        while (!m_extra_views.empty())
        {
            close_extra_view(m_extra_views.size() - 1);
        }

        get_setting<StreamingSettings>().mode = StreamingMode::Enabled;
        m_mode_restore_time = NEVER_TIME;
        m_paused = true;

        MIZU_LOG_INFO("[StreamingStressTest] drain: destroyed={} instances and sponza, scenario paused", destroyed);
    }

    void run_recycle()
    {
        const uint32_t asset_index = random_uint(0, static_cast<uint32_t>(SPAWNABLE_ASSETS.size() - 1));

        uint32_t count = 0;
        for (size_t i = m_instances.size(); i > 0; --i)
        {
            if (m_instances[i - 1].asset_index == asset_index)
            {
                destroy_instance(i - 1);
                ++count;
            }
        }

        const uint32_t respawn_count = std::max(count, 1u);
        for (uint32_t i = 0; i < respawn_count; ++i)
        {
            spawn_instance(asset_index, random_spawn_position(), false);
        }

        ++m_stats.recycles;

        MIZU_LOG_INFO(
            "[StreamingStressTest] recycle: asset={} destroyed={} respawned={}",
            SPAWNABLE_ASSETS[asset_index].path,
            count,
            respawn_count);
    }

    //
    // Camera
    //

    void start_camera_pattern(CameraPattern pattern)
    {
        m_camera_pattern = pattern;
        m_camera_pattern_start_time = m_time;
        m_next_camera_event_time = m_time;

        m_camera_focus = random_focus_point();
        m_camera_anchor = random_in_box(SPAWN_VOLUME_MIN, SPAWN_VOLUME_MAX);
        m_camera_direction = random_direction();
        m_whip_yaw = random_float(0.0f, glm::two_pi<float>());
        m_whip_pitch = 0.0f;

        m_scripted_camera.fov = DEFAULT_FOV;
    }

    void teleport_camera()
    {
        const uint32_t kind = random_uint(0, 2);

        if (kind == 2)
        {
            m_scripted_camera.position = random_in_box(SPAWN_VOLUME_MIN, SPAWN_VOLUME_MAX);
            m_scripted_camera.rotation = look_rotation(random_direction());
            return;
        }

        const glm::vec3 focus = random_focus_point();
        const glm::vec3 direction = random_direction();
        const float distance = kind == 0 ? random_float(0.5f, 3.0f) : random_float(100.0f, 300.0f);

        m_scripted_camera.position = focus - direction * distance;
        m_scripted_camera.rotation = look_rotation(direction);
    }

    void update_scripted_camera()
    {
        MIZU_PROFILE_SCOPED;

        if (m_time - m_camera_pattern_start_time >= m_config->camera_pattern_duration)
        {
            const uint32_t next =
                (static_cast<uint32_t>(m_camera_pattern) + 1) % static_cast<uint32_t>(CameraPattern::Count);
            start_camera_pattern(static_cast<CameraPattern>(next));
        }

        const float t = static_cast<float>(m_time - m_camera_pattern_start_time);
        const bool camera_event = m_time >= m_next_camera_event_time;

        Camera& camera = m_scripted_camera;

        switch (m_camera_pattern)
        {
        case CameraPattern::Dolly: {
            const float distance = glm::mix(DOLLY_MIN_DISTANCE, DOLLY_MAX_DISTANCE, oscillate(t, DOLLY_FREQUENCY));
            camera.position = m_camera_focus - m_camera_direction * distance;
            camera.rotation = look_rotation(m_camera_direction);
            break;
        }
        case CameraPattern::Teleport: {
            if (camera_event)
            {
                teleport_camera();
                m_next_camera_event_time = m_time + m_config->teleport_interval;
            }
            break;
        }
        case CameraPattern::WhipTurn: {
            if (camera_event)
            {
                m_whip_yaw += glm::pi<float>() + random_float(-0.3f, 0.3f);
                m_whip_pitch = random_float(-0.3f, 0.3f);
                m_next_camera_event_time = m_time + m_config->whip_interval;
            }
            camera.position = m_camera_anchor;
            camera.rotation = glm::vec3(m_whip_pitch, m_whip_yaw, 0.0f);
            break;
        }
        case CameraPattern::Orbit: {
            const float angle = t * ORBIT_ANGULAR_SPEED;
            const float radius = glm::mix(5.0f, 80.0f, oscillate(t, 0.1f));
            const float height = glm::mix(1.0f, 12.0f, oscillate(t, 0.2f));
            camera.position = m_camera_focus + glm::vec3(std::cos(angle) * radius, height, std::sin(angle) * radius);
            camera.rotation = look_rotation(m_camera_focus - camera.position);
            break;
        }
        case CameraPattern::FovZoom: {
            camera.position = m_camera_anchor;
            camera.rotation = look_rotation(m_camera_focus - m_camera_anchor);
            camera.fov = glm::mix(FOV_ZOOM_MAX, FOV_ZOOM_MIN, oscillate(t, FOV_ZOOM_FREQUENCY));
            break;
        }
        case CameraPattern::Count:
            break;
        }
    }

    Camera get_main_camera() const
    {
        Camera camera = m_free_camera ? m_free_camera_controller->get_camera() : m_scripted_camera;
        camera.aspect = m_main_view_split ? m_aspect_ratio * 0.5f : m_aspect_ratio;
        camera.znear = CAMERA_ZNEAR;
        camera.zfar = CAMERA_ZFAR;

        return camera;
    }

    //
    // Views
    //

    Camera create_extra_view_camera(ViewPreset preset)
    {
        Camera camera{};
        camera.fov = DEFAULT_FOV;
        camera.aspect = preset == ViewPreset::SplitScreen ? m_aspect_ratio * 0.5f : m_aspect_ratio;
        camera.znear = CAMERA_ZNEAR;
        camera.zfar = CAMERA_ZFAR;

        const glm::vec3 focus = random_focus_point();
        const glm::vec3 direction = random_direction();

        switch (preset)
        {
        case ViewPreset::CloseUp:
        case ViewPreset::Tiny:
            camera.position = focus - direction * random_float(0.5f, 2.0f);
            camera.rotation = look_rotation(direction);
            break;
        case ViewPreset::SplitScreen:
            camera.position = focus - direction * random_float(60.0f, 150.0f);
            camera.rotation = look_rotation(direction);
            break;
        case ViewPreset::Opposite:
        case ViewPreset::Count:
            break;
        }

        return camera;
    }

    ViewportRect get_extra_view_viewport(ViewPreset preset, uint32_t slot) const
    {
        switch (preset)
        {
        case ViewPreset::SplitScreen:
            return ViewportRect{.offset = glm::vec2(0.5f, 0.0f), .extent = glm::vec2(0.5f, 1.0f)};
        case ViewPreset::Tiny:
            return ViewportRect{
                .offset = glm::vec2(VIEW_MARGIN, 1.0f - TINY_VIEW_SIZE - VIEW_MARGIN),
                .extent = glm::vec2(TINY_VIEW_SIZE)};
        case ViewPreset::CloseUp:
        case ViewPreset::Opposite:
        case ViewPreset::Count:
            break;
        }

        const float x = 1.0f - PIP_VIEW_SIZE - VIEW_MARGIN;
        const float y = slot == 0 ? 1.0f - PIP_VIEW_SIZE - VIEW_MARGIN : VIEW_MARGIN;

        return ViewportRect{.offset = glm::vec2(x, y), .extent = glm::vec2(PIP_VIEW_SIZE)};
    }

    void open_extra_view(ViewPreset preset, uint32_t slot)
    {
        ExtraView view{};
        view.preset = preset;
        view.camera = create_extra_view_camera(preset);

        if (chance(m_config->view_blink_chance))
        {
            view.close_frame = m_frame + 1;
        }
        else
        {
            view.close_time = m_time + random_float(MIN_VIEW_LIFETIME, MAX_VIEW_LIFETIME);
        }

        RenderViewDynamicState view_ds{};
        view_ds.viewport = get_extra_view_viewport(preset, slot);
        view_ds.layer = slot + 1;
        view_ds.camera = view.camera;

        view.handle = g_render_view_state_manager->sim_create(RenderViewStaticState{}, view_ds);

        if (preset == ViewPreset::SplitScreen)
        {
            m_main_view_split = true;
        }

        m_extra_views.push_back(view);
        ++m_stats.views_opened;
    }

    void close_extra_view(size_t index)
    {
        const ExtraView& view = m_extra_views[index];

        g_render_view_state_manager->sim_destroy(view.handle);

        if (view.preset == ViewPreset::SplitScreen)
        {
            m_main_view_split = false;
        }

        m_extra_views.erase(m_extra_views.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void update_views()
    {
        MIZU_PROFILE_SCOPED;

        for (size_t i = m_extra_views.size(); i > 0; --i)
        {
            const ExtraView& view = m_extra_views[i - 1];

            const bool expired = m_time >= view.close_time || m_frame >= view.close_frame;
            if (expired)
            {
                close_extra_view(i - 1);
            }
        }

        if (!m_extra_views.empty())
            return;

        if (m_time < m_next_view_time)
            return;

        const uint32_t count = random_uint(1, 2);
        for (uint32_t slot = 0; slot < count; ++slot)
        {
            ViewPreset preset = static_cast<ViewPreset>(random_uint(0, static_cast<uint32_t>(ViewPreset::Count) - 1));

            const bool split_unavailable = count > 1 || m_main_view_split;
            if (preset == ViewPreset::SplitScreen && split_unavailable)
            {
                preset = ViewPreset::CloseUp;
            }

            open_extra_view(preset, slot);
        }

        m_next_view_time = m_time + random_float(m_config->min_view_interval, m_config->max_view_interval);

        MIZU_LOG_INFO(
            "[StreamingStressTest] views opened: {} ({}{}{})",
            count,
            to_string(m_extra_views[0].preset),
            count > 1 ? ", " : "",
            count > 1 ? to_string(m_extra_views[1].preset) : "");
    }

    void update_render_views()
    {
        const Camera main_camera = get_main_camera();

        RenderViewDynamicState& main_view_ds = g_render_view_state_manager->sim_edit(m_main_view_handle);
        main_view_ds.camera = main_camera;
        main_view_ds.viewport = m_main_view_split
                                    ? ViewportRect{.offset = glm::vec2(0.0f), .extent = glm::vec2(0.5f, 1.0f)}
                                    : ViewportRect{};

        const glm::vec3 scene_center = (SPAWN_VOLUME_MIN + SPAWN_VOLUME_MAX) * 0.5f;

        for (ExtraView& view : m_extra_views)
        {
            if (view.preset == ViewPreset::Opposite)
            {
                const glm::vec3 offset = main_camera.position - scene_center;
                view.camera.position = scene_center + glm::vec3(-offset.x, offset.y, -offset.z);
                view.camera.rotation = main_camera.rotation + glm::vec3(0.0f, glm::pi<float>(), 0.0f);
            }

            view.camera.aspect = view.preset == ViewPreset::SplitScreen ? m_aspect_ratio * 0.5f : m_aspect_ratio;

            RenderViewDynamicState& view_ds = g_render_view_state_manager->sim_edit(view.handle);
            view_ds.camera = view.camera;
        }
    }

    //
    // Streaming mode
    //

    void set_streaming_mode(StreamingMode mode)
    {
        get_setting<StreamingSettings>().mode = mode;
        ++m_stats.mode_switches;

        MIZU_LOG_INFO("[StreamingStressTest] streaming mode: {}", to_string(mode));
    }

    void update_streaming_mode()
    {
        if (m_time >= m_mode_restore_time)
        {
            set_streaming_mode(StreamingMode::Enabled);
            m_mode_restore_time = NEVER_TIME;
            m_next_mode_time = m_time + m_config->mode_interval;
            return;
        }

        if (m_mode_restore_time != NEVER_TIME || m_time < m_next_mode_time)
            return;

        const bool full_residency = chance(0.5f);
        set_streaming_mode(full_residency ? StreamingMode::FullResidency : StreamingMode::Frozen);
        m_mode_restore_time = m_time + m_config->mode_hold;
    }

    //
    // Stats
    //

    void log_stats()
    {
        if (m_time < m_next_stats_log_time)
            return;

        m_next_stats_log_time = m_time + STATS_LOG_INTERVAL;

        MIZU_LOG_INFO(
            "[StreamingStressTest] t={:.1f}s preset={} camera={}{} instances={} views={} mode={} spawned={} "
            "despawned={} flickers={} waves={} recycles={} sponza_rebuilds={} views_opened={} mode_switches={}",
            m_time,
            m_config->name,
            m_free_camera ? "free" : to_string(m_camera_pattern),
            m_paused ? " (paused)" : "",
            m_instances.size(),
            m_extra_views.size() + 1,
            to_string(get_setting<StreamingSettings>().mode),
            m_stats.spawned,
            m_stats.despawned,
            m_stats.flickers,
            m_stats.waves,
            m_stats.recycles,
            m_stats.sponza_rebuilds,
            m_stats.views_opened,
            m_stats.mode_switches);
    }
};

class StreamingStressTestGame : public GameMain
{
  public:
    GameDescription get_game_description() const override
    {
        GameDescription desc{};
        desc.width = 1920;
        desc.height = 1080;

        return desc;
    }

    GameSimulation* create_game_simulation() const override { return new StreamingStressTestSimulation{}; }
};

GameMain* Mizu::create_game_main()
{
    return new StreamingStressTestGame{};
}
