#pragma once

#include <span>
#include <vector>

class RenderTestProvider;

class RenderTestsRegistry
{
  public:
    static RenderTestsRegistry& get();

    std::span<RenderTestProvider*> get_render_tests();

  private:
    std::vector<RenderTestProvider*> m_render_tests;
};
