#pragma once

#include <filesystem>
#include <vector>

#include "asset_cooker.h"

namespace Mizu
{

class FilesystemRequestSource : public IRequestSource
{
  public:
    bool init(const CookContext& context) override;

    uint32_t enumerate_n(uint32_t number, const CookContext& context, std::vector<ImportRequest>& outputs) override;

  private:
    class AssetMountEnumerator
    {
      public:
        AssetMountEnumerator(const AssetMount& mount);

        uint32_t enumerate_n(uint32_t number, const CookContext& context, std::vector<ImportRequest>& outputs);
        bool empty() const;

      private:
        const AssetMount& m_mount;

        std::filesystem::recursive_directory_iterator m_cursor{};
        std::filesystem::recursive_directory_iterator m_end{};
    };

    std::vector<AssetMountEnumerator> m_mount_enumerators{};
    uint32_t m_mount_enumerator_cursor = 0;

    std::vector<ImportRequest> m_pending_requests;
};

} // namespace Mizu
