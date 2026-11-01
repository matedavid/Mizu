#pragma once

#include <string_view>

#include "dx12_core.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

namespace debug
{

void begin_gpu_marker(ID3D12GraphicsCommandList* cmd, std::string_view label);
void end_gpu_marker(ID3D12GraphicsCommandList* cmd);

void set_resource_name(ID3D12Resource* resource, std::string_view name);
void set_resource_name(ID3D12Heap* heap, std::string_view name);

}; // namespace debug

#define DX12_DEBUG_BEGIN_GPU_MARKER(cmd, label) debug::begin_gpu_marker(cmd, label)
#define DX12_DEBUG_END_GPU_MARKER(cmd) debug::end_gpu_marker(cmd)

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name) debug::set_resource_name(resource, name)

#else

#define DX12_DEBUG_BEGIN_GPU_MARKER(cmd, label) \
    do                                          \
    {                                           \
        (void)cmd;                              \
        (void)label;                            \
    } while (false)

#define DX12_DEBUG_END_GPU_MARKER(cmd) \
    do                                 \
    {                                  \
        (void)cmd;                     \
    } while (false)

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name) \
    do                                               \
    {                                                \
        (void)resource;                              \
        (void)name;                                  \
    } while (false)

#endif

} // namespace Mizu::Dx12
