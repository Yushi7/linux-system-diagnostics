#include "sysdiag/cpu.hpp"
#include "sysdiag/memory.hpp"
#include "sysdiag/process.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

    struct Options
    {
        std::size_t iterations{30};
        std::chrono::milliseconds sample_interval{250};
        std::filesystem::path proc_root{"/proc"};
    };

    struct Summary
    {
        double mean{};
        double median{};
        double percentile_95{};
    };

    std::uint64_t parse_positive_integer(std::string_view value,
                                         std::string_view option,
                                         std::uint64_t maximum)
    {
        std::uint64_t parsed{};
        const auto result =
            std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (value.empty() || result.ec != std::errc{} ||
            result.ptr != value.data() + value.size() || parsed == 0 ||
            parsed > maximum)
        {
            throw std::invalid_argument(std::string(option) +
                                        " requires an integer in the range 1 to " +
                                        std::to_string(maximum));
        }
        return parsed;
    }

    Options parse_arguments(int argc, char *argv[])
    {
        Options options;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument{argv[index]};
            if (argument == "--help" || argument == "-h")
            {
                std::cout << "Usage: " << argv[0]
                          << " [--iterations N] [--sample-interval-ms N]"
                             " [--proc-root PATH]\n"
                             "Defaults: 30 iterations, 250 ms sample interval, /proc.\n"
                             "Reports reader latency and estimated monitoring work per sample;\n"
                             "the intentional sampling sleep is excluded from work time.\n";
                std::exit(0);
            }
            if (index + 1 >= argc)
            {
                throw std::invalid_argument("Missing value for " +
                                            std::string(argument));
            }

            const std::string_view value{argv[++index]};
            if (argument == "--iterations")
            {
                options.iterations = static_cast<std::size_t>(
                    parse_positive_integer(value, argument, 1000000));
            }
            else if (argument == "--sample-interval-ms")
            {
                options.sample_interval = std::chrono::milliseconds{
                    parse_positive_integer(value, argument, 60000)};
            }
            else if (argument == "--proc-root")
            {
                if (value.empty())
                {
                    throw std::invalid_argument("--proc-root cannot be empty");
                }
                options.proc_root = value;
            }
            else
            {
                throw std::invalid_argument("Unknown option: " +
                                            std::string(argument));
            }
        }
        return options;
    }

    template <typename Operation>
    double measure_microseconds(Operation &&operation)
    {
        const auto start = std::chrono::steady_clock::now();
        operation();
        const auto finish = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::micro>(finish - start).count();
    }

    Summary summarize(std::vector<double> samples)
    {
        std::sort(samples.begin(), samples.end());
        double sum{};
        for (const auto sample : samples)
        {
            sum += sample;
        }

        const auto middle = samples.size() / 2;
        const auto median = samples.size() % 2 == 0
                                ? (samples[middle - 1] + samples[middle]) / 2.0
                                : samples[middle];
        const auto percentile_index = static_cast<std::size_t>(
            std::ceil(0.95 * static_cast<double>(samples.size())) - 1.0);
        return {sum / static_cast<double>(samples.size()), median,
                samples[percentile_index]};
    }

    void print_summary(std::string_view name, const Summary &summary)
    {
        std::cout << std::left << std::setw(24) << name << std::right
                  << " mean " << std::setw(9) << summary.mean << " us, median "
                  << std::setw(9) << summary.median << " us, p95 " << std::setw(9)
                  << summary.percentile_95 << " us\n";
    }

} // namespace

int main(int argc, char *argv[])
{
    try
    {
        const auto options = parse_arguments(argc, argv);
        std::vector<double> cpu_samples;
        std::vector<double> memory_samples;
        std::vector<double> process_samples;
        std::vector<double> total_samples;
        cpu_samples.reserve(options.iterations);
        memory_samples.reserve(options.iterations);
        process_samples.reserve(options.iterations);
        total_samples.reserve(options.iterations);

        sysdiag::CpuTimes last_cpu{};
        sysdiag::MemoryStats last_memory{};
        std::size_t last_process_count{};

        const auto run_once = [&](bool record_samples)
        {
            const auto total_start = std::chrono::steady_clock::now();
            const auto cpu_time = measure_microseconds([&]
                                                       {
                const auto before =
                    sysdiag::read_cpu_times(options.proc_root / "stat");
                last_cpu = sysdiag::read_cpu_times(options.proc_root / "stat");
                static_cast<void>(
                    sysdiag::calculate_cpu_usage(before, last_cpu)); });
            const auto memory_time = measure_microseconds([&]
                                                          { last_memory = sysdiag::read_memory_stats(
                                                                options.proc_root / "meminfo"); });
            const auto process_time = measure_microseconds([&]
                                                           { last_process_count =
                                                                 sysdiag::list_processes(options.proc_root).size(); });
            const auto total_finish = std::chrono::steady_clock::now();

            if (record_samples)
            {
                cpu_samples.push_back(cpu_time);
                memory_samples.push_back(memory_time);
                process_samples.push_back(process_time);
                total_samples.push_back(
                    std::chrono::duration<double, std::micro>(total_finish -
                                                              total_start)
                        .count());
            }
        };

        run_once(false);
        for (std::size_t index = 0; index < options.iterations; ++index)
        {
            run_once(true);
        }

        const auto cpu_summary = summarize(std::move(cpu_samples));
        const auto memory_summary = summarize(std::move(memory_samples));
        const auto process_summary = summarize(std::move(process_samples));
        const auto total_summary = summarize(std::move(total_samples));
        const auto interval_microseconds =
            static_cast<double>(options.sample_interval.count()) * 1000.0;
        const auto overhead_percent =
            100.0 * total_summary.mean / interval_microseconds;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Benchmark: " << options.iterations << " iterations, "
                  << options.sample_interval.count() << " ms sample interval\n";
        print_summary("CPU stat read", cpu_summary);
        print_summary("Memory info read", memory_summary);
        print_summary("Process enumeration", process_summary);
        print_summary("Total monitoring work", total_summary);
        std::cout << "Estimated monitoring work per sample: " << overhead_percent
                  << "% of the configured interval\n";
        std::cout << "Last iteration observed " << last_process_count
                  << " processes and " << last_memory.total_kib
                  << " KiB total memory.\n";
        if (last_cpu.user + last_cpu.system + last_cpu.idle == 0)
        {
            throw std::runtime_error("CPU counters contained no measurable ticks");
        }
        return 0;
    }
    catch (const std::invalid_argument &error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return 1;
    }
}