#pragma once

#include "core/layer.h"

namespace Mizu
{

class ImGuiLayer : public Layer
{
  public:
    ImGuiLayer();
    ~ImGuiLayer();

    void on_update(double ts) override;
    virtual void on_update_impl(double ts) = 0;
};

} // namespace Mizu
