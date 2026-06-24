#include "runner/render_tests_registry.h"

RenderTestsRegistry& RenderTestsRegistry::get()
{
    static RenderTestsRegistry instance;
    return instance;
}

std::span<RenderTestProvider*> RenderTestsRegistry::get_render_tests()
{
    return m_render_tests;
}
