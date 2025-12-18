#pragma once

#include <string_view>

#include "dx12_core.h"

namespace Mizu::Dx12
{

#if MIZU_DX12_VALIDATIONS_ENABLED

class Dx12Debug
{
  public:
    static void set_resource_name(ID3D12Resource* resource, std::string_view name);
};

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name) Dx12Debug::set_resource_name(resource, name)

#else

#define DX12_DEBUG_SET_RESOURCE_NAME(resource, name)

#endif

} // namespace Mizu::Dx12
