#pragma once

#include <cstdint>
#include <filesystem>

namespace sysdiag
{

    struct MemoryStats
    {
        std::uint64_t total_kib{};
        std::uint64_t used_kib{};
        std::uint64_t available_kib{};
        std::uint64_t free_kib{};
        bool available_is_estimated{};
    };

    [[nodiscard]] MemoryStats read_memory_stats(
        const std::filesystem::path &meminfo = "/proc/meminfo");

} // namespace sysdiag