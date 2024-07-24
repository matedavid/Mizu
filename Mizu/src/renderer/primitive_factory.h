#pragma once

#include <memory>

#include "renderer/mesh.h"

namespace Mizu {

class PrimitiveFactory {
  public:
    static void clean();

    static std::shared_ptr<Mesh> get_cube();
};

} // namespace Mizu
