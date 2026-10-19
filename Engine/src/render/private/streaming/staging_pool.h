#pragma once

#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace Mizu
{

struct StagingBlock
{
    uint8_t* data = nullptr;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint64_t reserved = 0;

    std::span<uint8_t> as_span() const { return std::span<uint8_t>(data, size); }
    bool is_valid() const { return data != nullptr && size > 0; }
};

class StagingPool
{
    static constexpr uint64_t ALIGNMENT = alignof(std::max_align_t);

    static_assert(ALIGNMENT > 0, "StagingPool alignment must be greater than 0");
    static_assert(ALIGNMENT % 2 == 0, "StagingPool alignment must be a power of 2");

  public:
    StagingPool(uint64_t capacity);

    std::optional<StagingBlock> allocate(uint64_t size, uint64_t alignment = ALIGNMENT);
    void free(const StagingBlock& block);

  private:
    std::vector<uint8_t> m_buffer{};

    struct FreeBlock
    {
        uint64_t offset;
        uint64_t size;
    };

    std::mutex m_mutex;
    std::vector<FreeBlock> m_free_blocks{};
};

} // namespace Mizu