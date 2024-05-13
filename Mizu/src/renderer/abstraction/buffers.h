#ifndef MIZU_BUFFERS_H
#define MIZU_BUFFERS_H

#include <cassert>
#include <memory>
#include <vector>

#include "renderer.h"

#include "utility/logging.h"

namespace Mizu {

// Forward declarations
namespace Vulkan {

class VulkanVertexBuffer;
class VulkanUniformBuffer;

} // namespace Vulkan

namespace OpenGL {

class OpenGLVertexBuffer;
class OpenGLUniformBuffer;

} // namespace OpenGL

class VertexBuffer {
  public:
    struct Layout {
        enum class Type {
            Float,
            UnsignedInt,
        };

        Type type;
        uint32_t count;
        bool normalized;
    };

    virtual ~VertexBuffer() = default;

    template <typename T>
    static std::shared_ptr<VertexBuffer> create(const std::vector<T>& data, const std::vector<Layout>& layout) {
        const auto count = data.size();
        const auto size = data.size() * sizeof(T);

        switch (get_config().graphics_api) {
        case GraphicsAPI::Vulkan:
            return std::make_shared<Vulkan::VulkanVertexBuffer>(data.data(), count, size);
        case GraphicsAPI::OpenGL:
            return std::make_shared<OpenGL::OpenGLVertexBuffer>(data.data(), count, size, layout);
        }
    }

    [[nodiscard]] virtual uint32_t count() const = 0;
};

class IndexBuffer {
  public:
    virtual ~IndexBuffer() = default;

    static std::shared_ptr<IndexBuffer> create(const std::vector<uint32_t>& data);

    [[nodiscard]] virtual uint32_t count() const = 0;
};

class UniformBuffer {
  public:
    virtual ~UniformBuffer() = default;

    template <typename T>
    static std::shared_ptr<UniformBuffer> create() {
        const uint32_t size = sizeof(T);
        switch (get_config().graphics_api) {
        case GraphicsAPI::Vulkan:
            return std::dynamic_pointer_cast<UniformBuffer>(std::make_shared<Vulkan::VulkanUniformBuffer>(size));
        case GraphicsAPI::OpenGL:
            return std::dynamic_pointer_cast<UniformBuffer>(std::make_shared<OpenGL::OpenGLUniformBuffer>(size));
        }
    }

    template <typename T>
    void update(const T& data) {
        assert(sizeof(data) == size() && "Size of data must be equal to size of creation data");
        set_data(&data);
    }

    virtual void set_data(const void* data) = 0;
    virtual void set_data(const void* data, uint32_t size, uint32_t offset_bytes) = 0;

    [[nodiscard]] virtual uint32_t size() const = 0;
};

} // namespace Mizu

#endif

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffers.h"
