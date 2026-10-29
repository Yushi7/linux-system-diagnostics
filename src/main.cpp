#include "sysdiag/cpu.hpp"
#include "sysdiag/cli.hpp"
#include "sysdiag/memory.hpp"
#include "sysdiag/process.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace
{

    double kib_to_mib(std::uint64_t kib)
    {
        return static_cast<double>(kib) / 1024.0;
    }

    void report_section_error(const char *section, const std::exception &error)
    {
        std::cerr << "error [" << section << "]: " << error.what() << '\n';
    }

} // namespace

int main(int argc, char *argv[])
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h")
        {
            std::cout << sysdiag::help_message(argv[0]);
            return 0;
        }
    }

    sysdiag::Options options;
    try
    {
        options = sysdiag::parse_arguments(argc, argv);
    }
    catch (const std::invalid_argument &error)
    {
        std::cerr << "error: " << error.what() << '\n'
                  << sysdiag::help_message(argv[0]);
        return 2;
    }

    bool had_error{};
    std::cout << std::fixed << std::setprecision(1);

    if (options.show_cpu)
    {
        try
        {
            const auto cpu_before =
                sysdiag::read_cpu_times(options.proc_root / "stat");
            std::this_thread::sleep_for(options.sample_interval);
            const auto cpu_after =
                sysdiag::read_cpu_times(options.proc_root / "stat");
            const auto cpu_usage =
                sysdiag::calculate_cpu_usage(cpu_before, cpu_after);

            std::cout << "CPU usage (" << options.sample_interval.count()
                      << " ms sample): " << cpu_usage.overall_percent << "%\n";
            std::cout << "CPU time breakdown: user " << cpu_usage.user_percent
                      << "%, system " << cpu_usage.system_percent << "%, idle "
                      << cpu_usage.idle_percent << "%, iowait "
                      << cpu_usage.iowait_percent << "%\n\n";
        }
        catch (const std::exception &error)
        {
            report_section_error("cpu", error);
            had_error = true;
        }
    }

    if (options.show_memory)
    {
        try
        {
            const auto memory =
                sysdiag::read_memory_stats(options.proc_root / "meminfo");

            std::cout << "Memory (MiB):\n";
            std::cout << "  Total:     " << kib_to_mib(memory.total_kib) << '\n';
            std::cout << "  Used:      " << kib_to_mib(memory.used_kib) << '\n';
            std::cout << "  Available: " << kib_to_mib(memory.available_kib);
            if (memory.available_is_estimated)
            {
                std::cout << " (estimated)";
            }
            std::cout << '\n';
            std::cout << "  Free:      " << kib_to_mib(memory.free_kib) << "\n\n";
        }
        catch (const std::exception &error)
        {
            report_section_error("memory", error);
            had_error = true;
        }
    }

    if (options.show_processes)
    {
        try
        {
            const auto processes = sysdiag::list_processes(options.proc_root);
            const auto displayed_count =
                options.max_processes == 0
                    ? processes.size()
                    : std::min(processes.size(), options.max_processes);

            std::cout << "Processes (" << processes.size() << "):\n";
            if (displayed_count < processes.size())
            {
                std::cout << "Showing first " << displayed_count << " processes.\n";
            }
            std::cout << std::left << std::setw(8) << "PID" << std::setw(24)
                      << "NAME" << "RSS (MiB)\n";
            for (std::size_t index = 0; index < displayed_count; ++index)
            {
                const auto &process = processes[index];
                std::cout << std::left << std::setw(8) << process.pid
                          << std::setw(24) << process.name;
                if (process.resident_memory_kib)
                {
                    std::cout << std::right << std::setw(10)
                              << kib_to_mib(*process.resident_memory_kib) << '\n';
                }
                else
                {
                    std::cout << std::right << std::setw(10) << "unavailable"
                              << '\n';
                }
            }
        }
        catch (const std::exception &error)
        {
            report_section_error("processes", error);
            had_error = true;
        }
    }

    return had_error ? 1 : 0;
}