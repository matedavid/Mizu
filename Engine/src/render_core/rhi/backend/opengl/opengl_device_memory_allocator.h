#pragma once

#include "render_core/rhi/device_memory_allocator.h"

#include "render_core/rhi/backend/opengl/opengl_buffer_resource.h"
#include "render_core/rhi/backend/opengl/opengl_image_resource.h"

namespace Mizu::OpenGL
{

class OpenGLBaseDeviceMemoryAllocator : public BaseDeviceMemoryAllocator
{
  public:
    OpenGLBaseDeviceMemoryAllocator() = default;
    ~OpenGLBaseDeviceMemoryAllocator() override = default;

    Allocation allocate_buffer_resource(const BufferResource& buffer) override;
    Allocation allocate_image_resource(const ImageResource& image) override;

    void release(Allocation id) override;
};

//
//
//

class OpenGLTransientImageResource : public TransientImageResource
{
  public:
    OpenGLTransientImageResource(const ImageDescription& desc);

    [[nodiscard]] uint64_t get_size() const override
    {
        // OpenGL aliasing is not implement, therefore size does not matter
        return 0;
    }
    [[nodiscard]] uint64_t get_alignment() const override
    {
        // OpenGL aliasing is not implement, therefore alignment does not matter
        return 0;
    }

    [[nodiscard]] std::shared_ptr<ImageResource> get_resource() const override { return m_resource; }

  private:
    std::shared_ptr<OpenGLImageResource> m_resource;
};

class OpenGLTransientBufferResource : public TransientBufferResource
{
  public:
    OpenGLTransientBufferResource(const BufferDescription& desc);

    [[nodiscard]] uint64_t get_size() const override
    {
        // OpenGL aliasing is not implement, therefore size does not matter
        return 0;
    }
    [[nodiscard]] uint64_t get_alignment() const override
    {
        // OpenGL aliasing is not implement, therefore alignment does not matter
        return 0;
    }

    [[nodiscard]] std::shared_ptr<BufferResource> get_resource() const override { return m_resource; }

  private:
    std::shared_ptr<OpenGLBufferResource> m_resource;
};

class OpenGLRenderGraphDeviceMemoryAllocator : public RenderGraphDeviceMemoryAllocator
{
  public:
    OpenGLRenderGraphDeviceMemoryAllocator() = default;
    ~OpenGLRenderGraphDeviceMemoryAllocator() override = default;

    void allocate_image_resource(const TransientImageResource& resource, size_t offset) override;
    void allocate_buffer_resource(const TransientBufferResource& resource, size_t offset) override;

    void allocate() override;

    size_t get_size() const override { return 0; }
};

} // namespace Mizu::OpenGL