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

class VertexBuffer {
  public:
    virtual ~VertexBuffer() = default;

    template <typename T>
    static std::shared_ptr<VertexBuffer> create(const std::vector<T>& data) {
        switch (get_config().graphics_api) {
        case GraphicsAPI::Vulkan:
            return std::make_shared<Vulkan::VulkanVertexBuffer>(data);
        }
    }

    virtual void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const = 0;

    [[nodiscard]] virtual uint32_t count() const = 0;
};

class IndexBuffer {
  public:
    virtual ~IndexBuffer() = default;

    static std::shared_ptr<IndexBuffer> create(const std::vector<uint32_t>& data);

    virtual void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const = 0;
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

#include "backend/vulkan/vulkan_buffers.h"
