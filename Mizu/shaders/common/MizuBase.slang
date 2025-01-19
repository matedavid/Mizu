#define MIZU_COMBINED_IMAGE_SAMPLER(texType, name, set, _binding) \
    [[vk::binding(_binding, set)]]                                \
    texType name : register(t##_binding, space##set);             \
    [[vk::binding(_binding, set)]]                                \
    SamplerState _##name##Sampler : register(s##_binding, space##set)
