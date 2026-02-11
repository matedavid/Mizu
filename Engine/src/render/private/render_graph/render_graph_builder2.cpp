#include "render/render_graph/render_graph_builder2.h"

namespace Mizu
{

//
// RenderGraphPassBuilder2
//

RenderGraphPassBuilder2::RenderGraphPassBuilder2(RenderGraphBuilder2& builder, std::string_view name, uint32_t pass_idx)
    : m_builder(builder)
    , m_hint(RenderGraphPassHint::Raster)
    , m_name(name)
    , m_pass_idx(pass_idx)
{
}

void RenderGraphPassBuilder2::set_hint(RenderGraphPassHint hint)
{
    m_hint = hint;
}

RenderGraphResource RenderGraphPassBuilder2::read(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::Read);
}

RenderGraphResource RenderGraphPassBuilder2::write(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::Write);
}

RenderGraphResource RenderGraphPassBuilder2::attachment(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::Attachment);
}

RenderGraphResource RenderGraphPassBuilder2::copy_src(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::CopySrc);
}

RenderGraphResource RenderGraphPassBuilder2::copy_dst(RenderGraphResource resource)
{
    return add_resource_access(resource, RenderGraphResourceUsageBits::CopyDst);
}

std::span<const RenderGraphAccessRecord> RenderGraphPassBuilder2::get_access_records() const
{
    return m_accesses;
}

RenderGraphResource RenderGraphPassBuilder2::add_resource_access(
    RenderGraphResource resource,
    RenderGraphResourceUsageBits usage)
{
    RenderGraphResourceDescription& desc = m_builder.get_resource_desc(resource);
    desc.usage |= usage;
    desc.first_pass_idx = std::min(desc.first_pass_idx, m_pass_idx);
    desc.last_pass_idx = std::max(desc.last_pass_idx, m_pass_idx);

    RenderGraphAccessRecord record{};
    record.resource = resource;
    record.usage = usage;
    m_accesses.push_back(record);

    return resource;
}

//
// RenderGraphBuilder2
//

RenderGraphBuilder2::RenderGraphBuilder2() {}

RenderGraphResource RenderGraphBuilder2::create_buffer(BufferDescription desc)
{
    RenderGraphResource resource_ref{};
    resource_ref.id = m_resources.size();

    RenderGraphResourceDescription& resource_desc = m_resources.emplace_back();
    resource_desc.type = RenderGraphResourceType::Buffer;
    resource_desc.desc = std::move(desc);
    resource_desc.usage = RenderGraphResourceUsageBits::None;

    return resource_ref;
}

RenderGraphResource RenderGraphBuilder2::create_constant_buffer(uint64_t size, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = 0u;
    desc.usage = BufferUsageBits::ConstantBuffer;
    desc.name = std::move(name);

    return create_buffer(desc);
}

RenderGraphResource RenderGraphBuilder2::create_structured_buffer(uint64_t size, uint64_t stride, std::string name)
{
    BufferDescription desc{};
    desc.size = size;
    desc.stride = stride;
    desc.usage = BufferUsageBits::None;
    desc.name = std::move(name);

    return create_buffer(desc);
}

RenderGraphResource RenderGraphBuilder2::create_texture(ImageDescription desc)
{
    RenderGraphResource resource_ref{};
    resource_ref.id = m_resources.size();

    RenderGraphResourceDescription& resource_desc = m_resources.emplace_back();
    resource_desc.type = RenderGraphResourceType::Texture;
    resource_desc.desc = std::move(desc);
    resource_desc.usage = RenderGraphResourceUsageBits::None;

    return resource_ref;
}

RenderGraphResource RenderGraphBuilder2::create_texture2d(
    uint32_t width,
    uint32_t height,
    ImageFormat format,
    std::string name)
{
    ImageDescription desc{};
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.type = ImageType::Image2D;
    desc.format = format;
    desc.usage = ImageUsageBits::None;
    desc.name = std::move(name);

    return create_texture(desc);
}

void RenderGraphBuilder2::compile()
{
    MIZU_ASSERT(!m_passes.empty(), "Can't compile RenderGraph without passes");

    std::vector<BufferDescription> buffer_descriptions;
    std::vector<ImageDescription> image_descriptions;
    std::vector<AccelerationStructureDescription> acceleration_structure_descriptions;

    for (const RenderGraphResourceDescription& resource_desc : m_resources)
    {
        MIZU_LOG_INFO("Resource:");
        MIZU_LOG_INFO("  Type:     {}", render_graph_resource_type_to_string(resource_desc.type));
        MIZU_LOG_INFO("  Usage:    {}", static_cast<RenderGraphResourceUsageBitsType>(resource_desc.usage));
        MIZU_LOG_INFO("  Accesses: ({},{})", resource_desc.first_pass_idx, resource_desc.last_pass_idx);

        switch (resource_desc.type)
        {
        case RenderGraphResourceType::Buffer: {
            BufferDescription desc = resource_desc.buffer();
            desc.usage |= get_buffer_usage_bits(resource_desc.usage);
            buffer_descriptions.push_back(desc);
            break;
        }
        case RenderGraphResourceType::Texture: {
            ImageDescription desc = resource_desc.image();
            desc.usage |= get_image_usage_bits(resource_desc.usage);
            image_descriptions.push_back(desc);
            break;
        }
        case RenderGraphResourceType::AccelerationStructure: {
            AccelerationStructureDescription desc = resource_desc.acceleration_structure();
            acceleration_structure_descriptions.push_back(desc);
            break;
        }
        }
    }
}

BufferUsageBits RenderGraphBuilder2::get_buffer_usage_bits(RenderGraphResourceUsageBits usage)
{
    BufferUsageBits usage_bits = BufferUsageBits::None;

    if (usage & RenderGraphResourceUsageBits::Write)
        usage_bits |= BufferUsageBits::UnorderedAccess;

    if (usage & RenderGraphResourceUsageBits::CopySrc)
        usage_bits |= BufferUsageBits::TransferSrc;

    if (usage & RenderGraphResourceUsageBits::CopyDst)
        usage_bits |= BufferUsageBits::TransferDst;

    return usage_bits;
}

ImageUsageBits RenderGraphBuilder2::get_image_usage_bits(RenderGraphResourceUsageBits usage)
{
    ImageUsageBits usage_bits = ImageUsageBits::None;

    if (usage & RenderGraphResourceUsageBits::Read)
        usage_bits |= ImageUsageBits::Sampled;

    if (usage & RenderGraphResourceUsageBits::Write)
        usage_bits |= ImageUsageBits::UnorderedAccess;

    if (usage & RenderGraphResourceUsageBits::Attachment)
        usage_bits | ImageUsageBits::Attachment;

    if (usage & RenderGraphResourceUsageBits::CopySrc)
        usage_bits |= ImageUsageBits::TransferSrc;

    if (usage & RenderGraphResourceUsageBits::CopyDst)
        usage_bits |= ImageUsageBits::TransferDst;

    return usage_bits;
}

} // namespace Mizu