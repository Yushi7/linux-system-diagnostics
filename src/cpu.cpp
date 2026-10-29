#include "sysdiag/cpu.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sysdiag
{
    namespace
    {

        std::uint64_t difference(std::uint64_t after, std::uint64_t before) noexcept
        {
            return after >= before ? after - before : 0;
        }

        double percentage(std::uint64_t part, std::uint64_t total) noexcept
        {
            if (total == 0)
            {
                return 0.0;
            }
            return 100.0 * static_cast<double>(part) / static_cast<double>(total);
        }

    } // namespace

    CpuTimes read_cpu_times(const std::filesystem::path &proc_stat)
    {
        std::ifstream input(proc_stat);
        if (!input)
        {
            throw std::runtime_error("Unable to open " + proc_stat.string());
        }

        std::string line;
        if (!std::getline(input, line))
        {
            throw std::runtime_error("Unable to read aggregate CPU statistics from " +
                                     proc_stat.string());
        }

        std::istringstream row(line);
        std::string label;
        if (!(row >> label) || label != "cpu")
        {
            throw std::runtime_error("Missing aggregate CPU statistics in " +
                                     proc_stat.string());
        }

        std::vector<std::uint64_t> counters;
        std::uint64_t counter{};
        while (row >> counter)
        {
            counters.push_back(counter);
        }
        if (!row.eof() || counters.size() < 4)
        {
            throw std::runtime_error("Malformed aggregate CPU statistics in " +
                                     proc_stat.string());
        }

        const auto value_at = [&counters](std::size_t index)
        {
            return index < counters.size() ? counters[index] : 0;
        };

        return {value_at(0), value_at(1), value_at(2), value_at(3),
                value_at(4), value_at(5), value_at(6), value_at(7)};
    }

    CpuUsage calculate_cpu_usage(const CpuTimes &before,
                                 const CpuTimes &after) noexcept
    {
        const auto user = difference(after.user, before.user);
        const auto nice = difference(after.nice, before.nice);
        const auto system = difference(after.system, before.system);
        const auto idle = difference(after.idle, before.idle);
        const auto iowait = difference(after.iowait, before.iowait);
        const auto irq = difference(after.irq, before.irq);
        const auto softirq = difference(after.softirq, before.softirq);
        const auto steal = difference(after.steal, before.steal);

        const auto total = user + nice + system + idle + iowait + irq + softirq + steal;
        const auto busy = total - idle - iowait;
        const auto user_time = user + nice;
        const auto system_time = system + irq + softirq + steal;

        return {percentage(busy, total), percentage(user_time, total),
                percentage(system_time, total), percentage(idle, total),
                percentage(iowait, total)};
    }

} // namespace sysdiag