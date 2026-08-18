#pragma once

#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "asset_cooker.h"

namespace Mizu
{

class FilesystemCookRequestSource : public ICookRequestSource
{
  public:
    ~FilesystemCookRequestSource() override;

    template <typename T>
    void add_asset_importer()
    {
        static_assert(std::is_base_of_v<IAssetImporter, T>, "T must inherit from IAssetImporter");

        T* importer = new T{};
        m_importers.push_back(importer);

        for (std::string_view extension : importer->extensions())
        {
            if (m_extension_to_importer_map.contains(extension))
            {
                MIZU_LOG_WARNING("Importer for extension '{}' is already registered", extension);
                continue;
            }

            m_extension_to_importer_map.insert({extension, importer});
        }
    }

    void init(const CookContext& context) override;

    uint32_t enumerate_n(const CookContext& context, uint32_t number, std::vector<CookRequest>& outputs) override;

  private:
    std::vector<IAssetImporter*> m_importers{};
    std::unordered_map<std::string_view, IAssetImporter*> m_extension_to_importer_map{};

    class AssetMountEnumerator
    {
      public:
        AssetMountEnumerator(const AssetMount& mount);

        uint32_t enumerate_n(uint32_t number, std::vector<std::filesystem::path>& outputs);

        bool empty() const { return m_cursor == m_end; }
        const AssetMount& mount() const { return m_mount; }

      private:
        const AssetMount& m_mount;

        std::filesystem::recursive_directory_iterator m_cursor{};
        std::filesystem::recursive_directory_iterator m_end{};
    };

    std::vector<AssetMountEnumerator> m_mount_enumerators{};
    uint32_t m_enumerator_cursor = 0;

    std::vector<CookRequest> m_pending_requests{};

    uint32_t create_cook_requests(
        const AssetMount& mount,
        const CookContext& context,
        const std::filesystem::path& path,
        std::vector<CookRequest>& outputs);

    const IAssetImporter* find_importer(std::string_view extension) const;
};

} // namespace Mizu