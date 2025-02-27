#pragma once

#include <memory>
#include <unordered_map>

namespace Mizu
{

// Forward declarations
class SamplerState;
struct SamplingOptions;

class SamplerStateCache
{
  public:
    SamplerStateCache() = default;

    [[nodiscard]] std::shared_ptr<SamplerState> get_sampler_state(const SamplingOptions& options);

  private:
    std::unordered_map<size_t, std::shared_ptr<SamplerState>> m_cache;

    size_t hash(const SamplingOptions& options) const;
};

} // namespace Mizu