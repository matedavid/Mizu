StateManager2
---

- Sim side produces published ticks that the render side consumes in order.
- Render runs decoupled from sim. Render never waits for sim.
- Sim can run decoupled until the number of published but not fully consumed ticks reaches MaxTicksAhead. MaxTicksAhead is shared across all state manager implementations. At that point sim waits for render to finish consuming at least one published tick.
- DynamicState is a full snapshot for a handle at a given sim tick.
- Sim can create a handle, update a handle, and destroy a handle.
- Only non-empty sim ticks are published.
- last_produced_tick and last_consumed_tick are monotonic published-tick indices. Ring-buffer slot indices are internal implementation details.
- Render interpolates from the last fully consumed state toward the next target tick.
- Create and destroy events are only committed when their tick is fully consumed, meaning the interpolation factor for that tick reached 1.0.


### StateManager Config

```cpp
struct StateManagerConfig
{
    bool     Interpolate  = false;
    uint64_t MaxHandles   = 100;
};
```

### Shared Limits

`MaxTicksAhead` should not be part of `StateManagerConfig`. It is a global compile-time limit shared by all state manager implementations.

```cpp
static constexpr uint64_t MaxTicksAhead = 10;
```

Notes:

- Every `StateManager` uses the same `MaxTicksAhead` value.
- Backpressure is still evaluated per `StateManager` instance. The shared part is the limit value, not the produced or consumed counters.
- If the implementation remains templated and inline, this constant likely belongs in a shared internal header rather than a single `.cpp` file. The important design point is that it is not configurable per manager instance.

### Core Semantics

- `last_produced_tick` is the highest fully published tick visible to render.
- `last_consumed_tick` is the highest fully consumed tick. A tick is consumed only when render finished interpolating to it and committed all of its events.
- Both values are monotonic and shared across threads, so both should be `std::atomic<uint64_t>`.
- Empty sim steps do not publish a tick and do not advance either shared counter.
- Sim writes tick payload first and only then publishes `last_produced_tick` with release semantics.
- Render reads `last_produced_tick` with acquire semantics before touching newly published tick data.
- Render advances `last_consumed_tick` with release semantics only after a tick is fully consumed.

### Sim Production

```cpp
class StateManager
{
    void sim_begin_tick();

    Handle sim_create(StaticState, DynamicState);
    void sim_update(Handle, DynamicState);
    void sim_destroy(Handle);

    // Publishes the pending tick only if there were any events in this sim step.
    void sim_end_tick();
};
```

Sim flow:

1. `sim_begin_tick()` resets the pending tick for the current sim step.
2. `sim_create`, `sim_update`, and `sim_destroy` append or coalesce handle events into that pending tick.
3. `sim_end_tick()` returns immediately if the pending tick is empty.
4. If the pending tick is not empty, sim checks backlog as `last_produced_tick - last_consumed_tick`.
5. If backlog is `>= MaxTicksAhead`, sim waits until render fully consumed at least one published tick.
6. Once there is capacity, sim assigns `next_tick_idx = last_produced_tick + 1`, maps it to a ring slot, writes the full tick payload, and then publishes `last_produced_tick = next_tick_idx`.

### Tick Storage

```cpp
enum class HandleEventKind
{
    Create,
    Update,
    Destroy,
};

struct HandleTick
{
    uint64_t        handle_idx;
    HandleEventKind kind;
    StaticState     ss; // Used by Create
    DynamicState    ds; // Used by Create and Update
};

struct Tick
{
    uint64_t tick_idx = 0;         // Monotonic published tick index
    double   sim_time = 0.0;       // Sim timestamp for interpolation
    uint32_t num_handle_ticks = 0; // Valid HandleTicks in this tick
};

std::atomic<uint64_t> last_produced_tick = 0; // Written by sim, read by render
std::atomic<uint64_t> last_consumed_tick = 0; // Written by render, read by sim

Tick* tick_in_production = nullptr;
bool  tick_in_production_dirty = false;

// Ring buffer of published ticks.
std::array<Tick, MaxTicksAhead> m_ticks;

// Each tick slot owns a fixed slice of MaxHandles HandleTicks.
std::array<HandleTick, MaxTicksAhead * MaxHandles> m_handle_ticks;

// Pending sim-step coalescing. Invalid value means the handle has no entry in the current pending tick.
std::array<uint32_t, MaxHandles> m_pending_handle_tick_indices;
```

Notes:

- `HandleTick` is still needed, but it represents a single handle event inside a single published tick.
- `HandleTick` does not need linked `prev` or `next` pointers.
- Each ring slot owns a contiguous slice of `MaxHandles` entries inside `m_handle_ticks`.
- Repeated operations on the same handle within the same sim step should be coalesced into one `HandleTick` entry.

Suggested same-tick coalescing rules:

- `Create` then `Update` becomes one `Create` with the latest `DynamicState`.
- `Update` then `Update` becomes one `Update` with the latest `DynamicState`.
- `Create` then `Destroy` in the same sim step drops the event entirely.
- `Update` then `Destroy` becomes one `Destroy`.

### Render State

Render keeps only persistent committed per-handle state. Interpolation target data is derived from the next target tick rather than stored permanently per handle.

```cpp
struct RenderHandleState
{
    bool alive = false;

    DynamicState committed_ds{}; // Last fully consumed state for this handle
};

std::array<RenderHandleState, MaxHandles> m_render_handle_states;
double m_rend_last_consumed_time = 0.0;
```

Meaning:

- `RenderHandleState` should be stored as member storage inside each `StateManager` instance, indexed by handle id.
- `committed_ds` is the last fully committed render state for the handle.
- `StaticState` does not need to live inside `RenderHandleState`. It can remain in the state manager's normal static-state storage.
- The render side only needs to persist the time of the last consumed tick. The next target tick time can be read from the next published tick in the ring buffer.
- Create and Destroy are inferred from the next target tick's `HandleTick::kind` rather than stored as persistent per-handle flags.

### Render Consumption

```cpp
class StateManager
{
    void rend_apply_updates(double render_time);

    void rend_created(Handle, StaticState, DynamicState);
    void rend_updated(Handle, DynamicState);
    void rend_destroyed(Handle);
};
```

Render flow:

1. Render loads `last_produced_tick`.
2. If `last_consumed_tick == last_produced_tick`, there is no target tick and render uses committed state directly.
3. Otherwise, the next target tick is always `last_consumed_tick + 1`.
4. Render reads that target tick's timestamp and `HandleTick` entries directly from the published tick ring buffer.
5. For a given handle, if the target tick has no `HandleTick` entry, render uses the handle's `committed_ds` directly.
6. If the target tick does contain a `HandleTick` for a handle, render infers the target state and event kind from that entry.
7. When interpolation for the target tick reaches `1.0`, render commits that entire tick, updates `m_rend_last_consumed_time`, advances `last_consumed_tick`, and then repeats the same process for the next published tick if one exists.

### Interpolation and Ordered Catch-Up

- Render always consumes ticks in order. It never skips directly to the newest tick.
- If render is 4 ticks behind and the same handle was updated in all 4 ticks, render still processes those 4 ticks one by one.
- Each tick becomes the next interpolation target only after the previous target tick was fully consumed.
- This preserves all intermediate DynamicState snapshots in order.
- If render time has already advanced far enough, render may fully consume multiple outstanding ticks in a single frame before settling on the first tick whose interpolation factor is still below `1.0`.

Update behavior:

- For an `Update`, render interpolates from `committed_ds` to the tick's `ds`.
- When the interpolation factor reaches `1.0`, `committed_ds` becomes the tick's `ds`, `m_rend_last_consumed_time` becomes the tick time, and the tick is consumed.

Create behavior:

- A `Create` event is not committed immediately when the target tick becomes active.
- Until the target tick reaches interpolation factor `1.0`, the handle stays not visible on render.
- When the tick is fully consumed, render calls `rend_created`, sets `alive = true`, and initializes `committed_ds` to the created `ds`.

Destroy behavior:

- A `Destroy` event is not committed immediately when the target tick becomes active.
- Until the target tick reaches interpolation factor `1.0`, the handle stays alive with its `committed_ds`.
- When the tick is fully consumed, render calls `rend_destroyed`, sets `alive = false`, and the handle is removed from the render side.

Notes:

- This plan intentionally keeps detailed interpolation policy open for later. The important part for now is the ordering and commit model.
- Exact alpha computation can later be based on tick timestamps, fixed tick delta, or another render-time mapping.
- A dedicated `ActiveRenderTick` struct is not required. The current target tick can be derived from `last_consumed_tick + 1` whenever published work is available.
- If render needs faster lookup of whether the target tick touches a handle, it can build a temporary or cached lookup for that target tick as an implementation detail.
- The key invariant is that create and destroy become visible only when their tick is fully consumed.

### StateManagerRegulator

Backpressure should live inside each `StateManager`.

- Sim waits locally when published backlog reaches `MaxTicksAhead`.
- Render does not wait on sim.
- A separate `StateManagerRegulator` is only needed later if multiple state managers must share a global pacing policy.

### Testing

The new tests should live under `tests/Engine/state_manager/`.

- Add a new test source file at `tests/Engine/state_manager/base_state_manager_tests.cpp`.
- Do not create a `state_manager2` test folder. The old state manager will be deprecated, so the tests should already use the final generic path and file name.
- Reuse the existing `MizuTests` target in `tests/CMakeLists.txt` and the current Catch2 setup already used by the other engine tests.
- Follow the same style used in `tests/Engine/base/`: `#include <catch2/catch_all.hpp>`, `TEST_CASE`, `SECTION`, and a `[StateManager]` tag.

Test file architecture:

- Keep the first version self-contained inside `base_state_manager_tests.cpp`.
- Define fake test-only types directly in the `.cpp` file: `TestStaticState`, `TestDynamicState`, `TestHandle`, and `TestConfig`.
- `TestDynamicState` should stay very small and easy to assert on. One or two scalar fields plus `has_changed()` is enough.
- Create a test-only fake state manager or test harness in the `.cpp` file. It should wrap the real implementation under test and record render-side effects for assertions.
- Prefer a small local helper struct over large fixture hierarchies. The current test style in the repo is simple and inline, so the state manager tests should share boilerplate through local helpers instead of heavy fixture inheritance.

Suggested test-only helpers:

```cpp
struct TestStaticState
{
    uint32_t value = 0;
};

struct TestDynamicState
{
    float value = 0.0f;

    bool has_changed(const TestDynamicState& other) const
    {
        return value != other.value;
    }
};

MIZU_STATE_MANAGER_CREATE_HANDLE(TestHandle);

struct TestConfig
{
    static constexpr uint64_t MaxHandles = 8;
};

struct RecordedRenderEvent
{
    enum class Kind
    {
        Create,
        Update,
        Destroy,
    };

    Kind      kind;
    uint64_t  handle_idx;
    float     value;
    uint64_t  tick_idx;
};

struct TestStateManagerHarness
{
    // Owns the real state manager under test.
    // Exposes small helper methods for common sim and render operations.
};
```

The test harness should centralize repetitive operations such as:

- beginning and ending a sim tick
- publishing an empty tick
- publishing create, update, and destroy events
- advancing render time in a deterministic way
- fully consuming the current target tick
- reading committed render state for a handle
- recording create, update, and destroy render events if the implementation exposes them

General testing strategy:

- Keep most tests single-threaded and deterministic.
- Drive sim and render manually from one thread for the majority of the correctness coverage.
- Use real threads only for the behaviors that actually depend on cross-thread publication or blocking.
- Avoid timing-sensitive tests whenever possible. Prefer barriers, atomics, latches, or explicit handshakes over sleeps.
- Do not over-specify the final interpolation policy yet. The first tests should validate ordering, commit boundaries, visibility rules, and ring-buffer correctness.

Core deterministic tests that should exist:

- A non-empty sim tick publishes and advances `last_produced_tick`.
- An empty sim tick does not publish and does not advance shared counters.
- Repeated updates to the same handle in one sim step coalesce into a single `HandleTick` with the latest state.
- `Create` then `Update` in one sim step becomes one `Create` with the latest `DynamicState`.
- `Create` then `Destroy` in one sim step drops the event entirely.
- `Update` then `Destroy` in one sim step becomes one `Destroy`.
- Render consumes ticks strictly in order and never skips intermediate ticks.
- If render is several ticks behind for one handle, the intermediate ticks are still processed one by one in order.
- When a target tick reaches full consumption, the committed render state changes to that tick's state.
- A handle not touched by the current target tick keeps its committed render state.

Lifecycle and visibility tests that should exist:

- A `Create` is not visible before its tick is fully consumed.
- A `Create` becomes visible exactly when its tick is fully consumed.
- A `Destroy` does not remove the handle before its tick is fully consumed.
- A `Destroy` removes the handle exactly when its tick is fully consumed.
- A destroyed handle id is not reusable until the destroy tick has been fully consumed by render.

Ring-buffer and wraparound tests that should exist:

- Publishing and consuming more than `MaxTicksAhead` total ticks over time works correctly with slot reuse.
- Only the `[0, num_handle_ticks)` prefix of a tick slot is considered valid.
- Stale `HandleTick` data in a reused slot is ignored.
- Render does not observe corrupted history after ring-slot reuse.

Threaded smoke tests that should exist:

- Sim blocks when backlog reaches `MaxTicksAhead` and resumes once render fully consumes one published tick.
- Render only observes a new published tick after sim finished writing the tick payload and published `last_produced_tick`.
- A simple producer thread and consumer thread can publish and consume a sequence of ticks without deadlock or reordering.

Transition note:

- If the implementation is still temporarily named `base_state_manager2` while this work is in progress, the test file can include that header initially.
- The test file path and test file name should still remain `tests/Engine/state_manager/base_state_manager_tests.cpp` so the tests survive the final rename cleanly.

