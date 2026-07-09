#define MIZU_SRV_BINDING_OFFSET 0
#define MIZU_UAV_BINDING_OFFSET 16
#define MIZU_CBV_BINDING_OFFSET 32
#define MIZU_SAMPLER_BINDING_OFFSET 64

#define MIZU_VK_BINDING_SRV(_binding, _set)     [[vk::binding(_binding + MIZU_SRV_BINDING_OFFSET,     _set)]]
#define MIZU_VK_BINDING_UAV(_binding, _set)     [[vk::binding(_binding + MIZU_UAV_BINDING_OFFSET,     _set)]]
#define MIZU_VK_BINDING_CBV(_binding, _set)     [[vk::binding(_binding + MIZU_CBV_BINDING_OFFSET,     _set)]]
#define MIZU_VK_BINDING_SAMPLER(_binding, _set) [[vk::binding(_binding + MIZU_SAMPLER_BINDING_OFFSET, _set)]]
