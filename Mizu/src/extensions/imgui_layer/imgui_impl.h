#pragma once

#include <imgui.h>
#include <memory>

namespace Mizu
{

// Forward declarations
class Texture2D;
class Window;
class Semaphore;

class INativeImGuiImpl
{
  public:
    virtual ~INativeImGuiImpl() = default;

    virtual void new_frame() = 0;
    virtual void render_frame(ImDrawData* draw_data, std::shared_ptr<Semaphore> wait_semaphore) = 0;
    virtual void present_frame() = 0;

    [[nodiscard]] virtual ImTextureID add_texture(const Texture2D& texture) = 0;
    virtual void remove_texture(ImTextureID texture_id) = 0;
};

class ImGuiImpl
{
  public:
    static void initialize(std::shared_ptr<Window> window);
    static void shutdown();

    static void new_frame();
    static void render_frame(ImDrawData* draw_data, std::shared_ptr<Semaphore> wait_semaphore);
    static void present_frame();

    [[nodiscard]] static ImTextureID add_texture(const Texture2D& texture);
    static void remove_texture(ImTextureID texture_id);

    static void set_background_image(ImTextureID texture);

    static void present();
    static void present(std::shared_ptr<Semaphore> wait_semaphore);

  protected:
    static std::unique_ptr<INativeImGuiImpl> s_native_impl;
    static uint32_t m_num_references;
};

} // namespace Mizu
