#include "filesystem_cook_request_source.h"

#include <filesystem>
#include <format>
#include <string>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

FilesystemCookRequestSource::~FilesystemCookRequestSource()
{
    for (IAssetImporter* importer : m_importers)
    {
        delete importer;
    }
}

void FilesystemCookRequestSource::init(const CookContext& context)
{
    m_mount_enumerators.clear();
    m_enumerator_cursor = 0;
    m_pending_requests.clear();

    for (const AssetMount& mount : context.asset_mounts.get_asset_mounts())
    {
        MIZU_ASSERT(std::filesystem::exists(mount.path), "Asset mount path '{}' does not exist", mount.path.string());
        m_mount_enumerators.emplace_back(mount);
    }
}

uint32_t FilesystemCookRequestSource::enumerate_n(
    const CookContext& context,
    uint32_t number,
    std::vector<CookRequest>& outputs)
{
    std::vector<std::filesystem::path> paths{};

    uint32_t num_enumerated = 0;

    while (num_enumerated < number)
    {
        if (m_pending_requests.empty())
        {
            if (m_enumerator_cursor >= m_mount_enumerators.size())
                break;

            paths.clear();

            AssetMountEnumerator& enumerator = m_mount_enumerators[m_enumerator_cursor];
            enumerator.enumerate_n(1, paths);

            for (const std::filesystem::path& path : paths)
            {
                create_cook_requests(enumerator.mount(), context, path, m_pending_requests);
            }

            if (enumerator.empty())
            {
                m_enumerator_cursor += 1;
            }

            continue;
        }

        CookRequest request = m_pending_requests.back();
        m_pending_requests.pop_back();

        outputs.push_back(std::move(request));

        num_enumerated += 1;
    }

    return num_enumerated;
}

uint32_t FilesystemCookRequestSource::create_cook_requests(
    const AssetMount& mount,
    const CookContext& context,
    const std::filesystem::path& path,
    std::vector<CookRequest>& outputs)
{
    const std::string extension = path.extension().string();

    const IAssetImporter* importer = find_importer(extension);
    if (importer == nullptr)
    {
        return 0;
    }

    const std::string virtual_path =
        std::format("{}:{}", mount.name, std::filesystem::relative(path, mount.path).generic_string());

    const ImportRequest import_request{
        .physical_path = path,
        .virtual_path = virtual_path,
        .asset_mounts = context.asset_mounts,
    };

    return importer->import(import_request, outputs);
}

const IAssetImporter* FilesystemCookRequestSource::find_importer(std::string_view extension) const
{
    const auto it = m_extension_to_importer_map.find(extension);
    if (it == m_extension_to_importer_map.end())
        return nullptr;

    return it->second;
}

FilesystemCookRequestSource::AssetMountEnumerator::AssetMountEnumerator(const AssetMount& mount) : m_mount(mount)
{
    m_cursor = std::filesystem::recursive_directory_iterator(m_mount.path);
}

uint32_t FilesystemCookRequestSource::AssetMountEnumerator::enumerate_n(
    uint32_t number,
    std::vector<std::filesystem::path>& outputs)
{
    uint32_t num_enumerated = 0;

    for (; m_cursor != m_end && num_enumerated < number; ++m_cursor)
    {
        const std::filesystem::directory_entry& entry = *m_cursor;
        if (!entry.is_regular_file())
            continue;

        outputs.push_back(entry.path());
        num_enumerated += 1;
    }

    return num_enumerated;
}

} // namespace Mizu
