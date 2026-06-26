#include "runner/render_tests_registry.h"

RenderTestsRegistry::~RenderTestsRegistry()
{
    for (RenderTest* test : m_render_tests)
    {
        delete test;
    }
}

RenderTestsRegistry& RenderTestsRegistry::get()
{
    static RenderTestsRegistry instance;
    return instance;
}

void RenderTestsRegistry::register_render_test(RenderTest* test)
{
    m_render_tests.push_back(test);
}

std::span<RenderTest*> RenderTestsRegistry::get_render_tests()
{
    return m_render_tests;
}
