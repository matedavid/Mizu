#pragma once

#include <memory>
#include <string>

#include "renderer/abstraction/image.h"

namespace Mizu {

class Cubemap : public virtual IImage {
  public:
    struct Faces {
        std::string right;
        std::string left;
        std::string top;
        std::string bottom;
        std::string front;
        std::string back;
    };

    virtual ~Cubemap() = default;

    static std::shared_ptr<Cubemap> create(const ImageDescription& desc);
    static std::shared_ptr<Cubemap> create(const Faces& faces);
};

} // namespace Mizu