#pragma once

#include <filesystem>
#include <memory>

#include "renderer/abstraction/image.h"

namespace Mizu {

class Texture2D : public virtual IImage {
  public:
    virtual ~Texture2D() = default;

    [[nodiscard]] static std::shared_ptr<Texture2D> create(const ImageDescription& desc);
    [[nodiscard]] static std::shared_ptr<Texture2D> create(const std::filesystem::path& path, SamplingOptions sampling);
};

} // namespace Mizu
