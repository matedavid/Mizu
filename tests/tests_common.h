#pragma once

#include <catch2/catch_all.hpp>
#include <vector>

#include <Mizu/Mizu.h>

#define GENERATE_GRAPHICS_APIS()                                                       \
    GENERATE(values<std::pair<Mizu::GraphicsAPI, Mizu::BackendSpecificConfiguration>>( \
        {{Mizu::GraphicsAPI::Vulkan, Mizu::VulkanSpecificConfiguration{}},             \
         {Mizu::GraphicsAPI::OpenGL, Mizu::OpenGLSpecificConfiguration{.create_context = true}}}))
