#include "filesystem_request_source.h"

#include <format>
#include <string>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

bool FilesystemRequestSource::init(const CookContext& context)
{
    m_mount_enumerators.clear();
    m_mount_enumerator_cursor = 0;

    for (const AssetMount& mount : context.asset_mounts.get_asset_mounts())
    {
        MIZU_ASSERT(std::filesystem::exists(mount.path), "Asset mount path '{}' does not exist", mount.path.string());
        m_mount_enumerators.emplace_back(mount);
    }

    return true;
}

uint32_t FilesystemRequestSource::enumerate_n(uint32_t number, std::vector<ImportRequest>& outputs)
{
    uint32_t num_enumerated = 0;

    while (num_enumerated < number && m_mount_enumerator_cursor < m_mount_enumerators.size())
    {
        AssetMountEnumerator& enumerator = m_mount_enumerators[m_mount_enumerator_cursor];
        num_enumerated += enumerator.enumerate_n(number - num_enumerated, outputs);

        if (enumerator.empty())
        {
            m_mount_enumerator_cursor += 1;
        }
    }

    return num_enumerated;
}

FilesystemRequestSource::AssetMountEnumerator::AssetMountEnumerator(const AssetMount& mount) : m_mount(mount)
{
    m_cursor = std::filesystem::recursive_directory_iterator(m_mount.path);
}

static std::string get_virtual_path(std::filesystem::path path, const AssetMount& mount)
{
    // lexically_normal().generic_string() normalizes the path only one, right slash, for paths
    return std::format("{}:{}", mount.name, path.lexically_normal().generic_string());
}

uint32_t FilesystemRequestSource::AssetMountEnumerator::enumerate_n(
    uint32_t number,
    std::vector<ImportRequest>& outputs)
{
    uint32_t num_enumerated = 0;

    for (; m_cursor != m_end && num_enumerated < number; ++m_cursor)
    {
        const std::filesystem::directory_entry& entry = *m_cursor;
        if (!entry.is_regular_file())
            continue;

        std::filesystem::path relative_path;
        std::string virtual_path;
        try
        {
            relative_path = std::filesystem::relative(entry, m_mount.path);
            virtual_path = get_virtual_path(relative_path, m_mount);
        }
        catch (const std::exception& e)
        {
            MIZU_LOG_ERROR("Failed to enumerate path, exception: {}", e.what());
            continue;
        }

        outputs.push_back({
            .extension = entry.path().extension().string(),
            .path = entry.path(),
            .virtual_path = virtual_path,
            .payload = {},
        });

        num_enumerated += 1;
    }

    return num_enumerated;
}

bool FilesystemRequestSource::AssetMountEnumerator::empty() const
{
    return m_cursor == m_end;
}

} // namespace Mizu
