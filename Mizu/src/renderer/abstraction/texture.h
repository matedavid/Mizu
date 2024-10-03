#pragma once

#include <memory>

#include "renderer/abstraction/image.h"

namespace Mizu {

class Texture2D : public virtual IImage {
  public:
    virtual ~Texture2D() = default;

    [[nodiscard]] static std::shared_ptr<Texture2D> create(const ImageDescription& desc);
};

} // namespace Mizu
