#pragma once

#include <span>
#include <vector>

#include "runner/render_test.h"

class RenderTestsRegistry
{
  public:
    ~RenderTestsRegistry();

    static RenderTestsRegistry& get();

    void register_render_test(RenderTest* test);
    std::span<RenderTest*> get_render_tests();

  private:
    std::vector<RenderTest*> m_render_tests;
};

class RenderTestCallback
{
  public:
    RenderTestCallback(RenderTest* test) { RenderTestsRegistry::get().register_render_test(test); }
};

#define REGISTER_RENDER_TEST(_test_class) RenderTestCallback g_##_test_class##_callback(new _test_class{})
