#pragma once

#include "resource_group.h"

namespace Mizu::OpenGL {

class OpenGLResourceGroup : public ResourceGroup {
  public:
    OpenGLResourceGroup() = default;
    ~OpenGLResourceGroup() override = default;

    void add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) override;
    void add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) override;
};

} // namespace Mizu::OpenGL