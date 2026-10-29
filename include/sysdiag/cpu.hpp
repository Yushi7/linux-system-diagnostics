#pragma once

#include <cstdint>
#include <filesystem>

namespace sysdiag
{

    struct CpuTimes
    {
        std::uint64_t user{};
        std::uint64_t nice{};
        std::uint64_t system{};
        std::uint64_t idle{};
        std::uint64_t iowait{};
        std::uint64_t irq{};
        std::uint64_t softirq{};
        std::uint64_t steal{};
    };

    struct CpuUsage
    {
        double overall_percent{};
        double user_percent{};
        double system_percent{};
        double idle_percent{};
        double iowait_percent{};
    };

    [[nodiscard]] CpuTimes read_cpu_times(
        const std::filesystem::path &proc_stat = "/proc/stat");
    [[nodiscard]] CpuUsage calculate_cpu_usage(const CpuTimes &before,
                                               const CpuTimes &after) noexcept;

} // namespace sysdiag