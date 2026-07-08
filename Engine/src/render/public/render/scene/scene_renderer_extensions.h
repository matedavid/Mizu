#pragma once

#include <array>
#include <functional>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "base/debug/assert.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;

enum class SceneRendererExtensionPoint
{
    // Begin of the frame
    FrameBegin,

    // Executed after the gbuffers have been created
    PostGBuffer,

    // Before the Lighting has been applied
    PreLighting,

    // After the Lighting has been applied
    PostLighting,

    // After Transparencies have been applied
    PostTransparency,

    // For rendering debug information
    Debug,

    // End of the frame
    FrameEnd,

    Count,
};

constexpr size_t SceneRendererExtensionPointCount = static_cast<size_t>(SceneRendererExtensionPoint::Count);

using SceneRendererExtensionPointFunc = std::function<void(RenderGraphBuilder&, RenderGraphBlackboard&)>;

class SceneRendererExtensions
{
  public:
    static void register_extension(SceneRendererExtensionPoint point, SceneRendererExtensionPointFunc&& func);
    static void execute_extensions(
        SceneRendererExtensionPoint point,
        RenderGraphBuilder& builder,
        RenderGraphBlackboard& blackboard);

  private:
    using SceneRendererPointExtensions = std::vector<SceneRendererExtensionPointFunc>;
    std::array<SceneRendererPointExtensions, SceneRendererExtensionPointCount> m_extensions{};

    void register_extension_internal(SceneRendererExtensionPoint point, SceneRendererExtensionPointFunc&& func);
    void execute_extensions_internal(
        SceneRendererExtensionPoint point,
        RenderGraphBuilder& builder,
        RenderGraphBlackboard& blackboard);
};

class SceneRenderModule
{
  public:
    virtual ~SceneRenderModule() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;
};

class SceneRendererModuleContainer
{
  public:
    ~SceneRendererModuleContainer();

    template <typename T>
    void add_module()
    {
        static_assert(std::is_base_of_v<SceneRenderModule, T>, "Module needs to inherit from SceneRendererModule");

        T* mod = new T{};
        mod->init();

        m_modules_map.insert({get_hash<T>(), mod});
    }

    template <typename T>
    T* get_render_module_opt() const
    {
        const auto it = m_modules_map.find(get_hash<T>());
        if (it == m_modules_map.end())
            return nullptr;

        return it->second;
    }

    template <typename T>
    T& get_render_module() const
    {
        T* mod = get_render_module_opt<T>();
        MIZU_ASSERT(mod != nullptr, "Could not find SceneRenderModule with name '{}'", typeid(T).name());

        return *mod;
    }

  private:
    std::unordered_map<size_t, SceneRenderModule*> m_modules_map{};

    template <typename T>
    inline size_t get_hash() const
    {
        return typeid(T).hash_code();
    }
};

} // namespace Mizu