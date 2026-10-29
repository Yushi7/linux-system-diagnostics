#include "sysdiag/memory.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace sysdiag
{

    MemoryStats read_memory_stats(const std::filesystem::path &meminfo)
    {
        std::ifstream input(meminfo);
        if (!input)
        {
            throw std::runtime_error("Unable to open " + meminfo.string());
        }

        std::uint64_t total{};
        std::uint64_t free{};
        std::uint64_t available{};
        std::uint64_t buffers{};
        std::uint64_t cached{};
        bool has_total{};
        bool has_free{};
        bool has_available{};

        std::string line;
        while (std::getline(input, line))
        {
            std::istringstream row(line);
            std::string key;
            std::uint64_t value{};
            std::string unit;
            if (!(row >> key >> value))
            {
                continue;
            }

            if (key != "MemTotal:" && key != "MemFree:" &&
                key != "MemAvailable:" && key != "Buffers:" &&
                key != "Cached:")
            {
                continue;
            }
            if (!(row >> unit) || unit != "kB")
            {
                throw std::runtime_error("Malformed memory value in " +
                                         meminfo.string());
            }

            if (key == "MemTotal:")
            {
                total = value;
                has_total = true;
            }
            else if (key == "MemFree:")
            {
                free = value;
                has_free = true;
            }
            else if (key == "MemAvailable:")
            {
                available = value;
                has_available = true;
            }
            else if (key == "Buffers:")
            {
                buffers = value;
            }
            else if (key == "Cached:")
            {
                cached = value;
            }
        }

        if (!has_total || !has_free || total == 0 || free > total)
        {
            throw std::runtime_error("Missing or invalid total/free memory in " +
                                     meminfo.string());
        }

        if (!has_available)
        {
            const auto maximum = std::numeric_limits<std::uint64_t>::max();
            const auto free_and_buffers =
                free > maximum - buffers ? maximum : free + buffers;
            const auto estimated =
                free_and_buffers > maximum - cached ? maximum : free_and_buffers + cached;
            available = std::min(total, estimated);
        }
        available = std::min(available, total);

        return {total, total - available, available, free, !has_available};
    }

} // namespace sysdiag