#include "sysdiag/cli.hpp"
#include "sysdiag/cpu.hpp"
#include "sysdiag/memory.hpp"
#include "sysdiag/process.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

    class TemporaryDirectory
    {
    public:
        TemporaryDirectory()
        {
            std::random_device random;
            const auto base = std::filesystem::temp_directory_path();
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                const auto candidate =
                    base / ("linux-sysdiag-test-" + std::to_string(random()) +
                            "-" + std::to_string(attempt));
                std::error_code error;
                if (std::filesystem::create_directory(candidate, error))
                {
                    path_ = candidate;
                    return;
                }
                if (error && error != std::errc::file_exists)
                {
                    throw std::filesystem::filesystem_error(
                        "Unable to create test directory", candidate, error);
                }
            }
            throw std::runtime_error("Unable to create a unique test directory");
        }

        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        TemporaryDirectory(const TemporaryDirectory &) = delete;
        TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

        [[nodiscard]] const std::filesystem::path &path() const noexcept
        {
            return path_;
        }

        void write(const std::filesystem::path &relative_path,
                   const std::string &contents) const
        {
            const auto file_path = path_ / relative_path;
            std::filesystem::create_directories(file_path.parent_path());
            std::ofstream output(file_path);
            if (!output || !(output << contents))
            {
                throw std::runtime_error("Unable to write test fixture " +
                                         file_path.string());
            }
        }

    private:
        std::filesystem::path path_;
    };

    sysdiag::Options parse_options(std::initializer_list<std::string> arguments)
    {
        std::vector<std::string> storage{"linux-sysdiag"};
        storage.insert(storage.end(), arguments.begin(), arguments.end());

        std::vector<char *> argv;
        argv.reserve(storage.size());
        for (auto &argument : storage)
        {
            argv.push_back(argument.data());
        }
        return sysdiag::parse_arguments(static_cast<int>(argv.size()), argv.data());
    }

} // namespace

TEST(CliTest, DefaultsToAllSections)
{
    const auto options = parse_options({});

    EXPECT_TRUE(options.show_cpu);
    EXPECT_TRUE(options.show_memory);
    EXPECT_TRUE(options.show_processes);
    EXPECT_EQ(options.sample_interval, std::chrono::milliseconds{250});
    EXPECT_EQ(options.proc_root, std::filesystem::path{"/proc"});
    EXPECT_EQ(options.max_processes, 0U);
}

TEST(CliTest, SelectsAndCombinesRequestedSections)
{
    const auto options = parse_options({"--cpu", "--processes"});

    EXPECT_TRUE(options.show_cpu);
    EXPECT_FALSE(options.show_memory);
    EXPECT_TRUE(options.show_processes);
}

TEST(CliTest, ParsesValuesForSamplingProcRootAndProcessLimit)
{
    const auto options = parse_options(
        {"--memory", "--sample-interval-ms", "1500", "--proc-root",
         "/tmp/proc fixture", "--max-processes", "12"});

    EXPECT_FALSE(options.show_cpu);
    EXPECT_TRUE(options.show_memory);
    EXPECT_FALSE(options.show_processes);
    EXPECT_EQ(options.sample_interval, std::chrono::milliseconds{1500});
    EXPECT_EQ(options.proc_root, std::filesystem::path{"/tmp/proc fixture"});
    EXPECT_EQ(options.max_processes, 12U);
}

TEST(CliTest, RejectsInvalidValuesAndUnknownOptions)
{
    EXPECT_THROW(parse_options({"--sample-interval-ms", "0"}),
                 std::invalid_argument);
    EXPECT_THROW(parse_options({"--sample-interval-ms", "60001"}),
                 std::invalid_argument);
    EXPECT_THROW(parse_options({"--max-processes", "not-a-number"}),
                 std::invalid_argument);
    EXPECT_THROW(parse_options({"--proc-root"}), std::invalid_argument);
    EXPECT_THROW(parse_options({"--unknown"}), std::invalid_argument);
}

TEST(CpuTest, ReadsAggregateCountersAndIgnoresPerCoreRows)
{
    TemporaryDirectory directory;
    directory.write("stat", "cpu 100 20 30 400 5 6 7 8 9 10\n"
                            "cpu0 50 10 15 200\n");

    const auto times = sysdiag::read_cpu_times(directory.path() / "stat");

    EXPECT_EQ(times.user, 100U);
    EXPECT_EQ(times.nice, 20U);
    EXPECT_EQ(times.system, 30U);
    EXPECT_EQ(times.idle, 400U);
    EXPECT_EQ(times.iowait, 5U);
    EXPECT_EQ(times.irq, 6U);
    EXPECT_EQ(times.softirq, 7U);
    EXPECT_EQ(times.steal, 8U);
}

TEST(CpuTest, RejectsMissingAndMalformedStatistics)
{
    TemporaryDirectory directory;
    EXPECT_THROW(static_cast<void>(sysdiag::read_cpu_times(directory.path() / "missing")),
                 std::runtime_error);

    directory.write("stat", "cpu 1 2 invalid 4\n");
    EXPECT_THROW(static_cast<void>(sysdiag::read_cpu_times(directory.path() / "stat")),
                 std::runtime_error);

    directory.write("stat", "cpu0 1 2 3 4\n");
    EXPECT_THROW(static_cast<void>(sysdiag::read_cpu_times(directory.path() / "stat")),
                 std::runtime_error);
}

TEST(CpuTest, CalculatesUsageFromCounterDeltas)
{
    const sysdiag::CpuTimes before{};
    const sysdiag::CpuTimes after{100, 0, 50, 800, 50, 0, 0, 0};

    const auto usage = sysdiag::calculate_cpu_usage(before, after);

    EXPECT_DOUBLE_EQ(usage.overall_percent, 15.0);
    EXPECT_DOUBLE_EQ(usage.user_percent, 10.0);
    EXPECT_DOUBLE_EQ(usage.system_percent, 5.0);
    EXPECT_DOUBLE_EQ(usage.idle_percent, 80.0);
    EXPECT_DOUBLE_EQ(usage.iowait_percent, 5.0);
}

TEST(CpuTest, HandlesZeroOrDecreasingCounters)
{
    const auto zero_delta =
        sysdiag::calculate_cpu_usage(sysdiag::CpuTimes{}, sysdiag::CpuTimes{});
    const auto decreasing = sysdiag::calculate_cpu_usage(
        sysdiag::CpuTimes{10, 0, 0, 0, 0, 0, 0, 0}, sysdiag::CpuTimes{});

    EXPECT_DOUBLE_EQ(zero_delta.overall_percent, 0.0);
    EXPECT_DOUBLE_EQ(decreasing.overall_percent, 0.0);
}

TEST(MemoryTest, ReadsRequiredAndAvailableMemoryValues)
{
    TemporaryDirectory directory;
    directory.write("meminfo", "MemTotal: 1000 kB\n"
                               "MemFree: 100 kB\n"
                               "MemAvailable: 600 kB\n");

    const auto memory = sysdiag::read_memory_stats(directory.path() / "meminfo");

    EXPECT_EQ(memory.total_kib, 1000U);
    EXPECT_EQ(memory.free_kib, 100U);
    EXPECT_EQ(memory.available_kib, 600U);
    EXPECT_EQ(memory.used_kib, 400U);
    EXPECT_FALSE(memory.available_is_estimated);
}

TEST(MemoryTest, EstimatesAvailableWhenKernelOmitsIt)
{
    TemporaryDirectory directory;
    directory.write("meminfo", "MemTotal: 1000 kB\n"
                               "MemFree: 100 kB\n"
                               "Buffers: 50 kB\n"
                               "Cached: 200 kB\n");

    const auto memory = sysdiag::read_memory_stats(directory.path() / "meminfo");

    EXPECT_EQ(memory.available_kib, 350U);
    EXPECT_EQ(memory.used_kib, 650U);
    EXPECT_TRUE(memory.available_is_estimated);
}

TEST(MemoryTest, RejectsMissingRequiredValuesAndMalformedUnits)
{
    TemporaryDirectory directory;
    EXPECT_THROW(static_cast<void>(sysdiag::read_memory_stats(directory.path() / "missing")),
                 std::runtime_error);

    directory.write("meminfo", "MemTotal: 1000 kB\n");
    EXPECT_THROW(static_cast<void>(sysdiag::read_memory_stats(directory.path() / "meminfo")),
                 std::runtime_error);

    directory.write("meminfo", "MemTotal: 1000 MB\nMemFree: 100 kB\n");
    EXPECT_THROW(static_cast<void>(sysdiag::read_memory_stats(directory.path() / "meminfo")),
                 std::runtime_error);

    directory.write("meminfo", "MemTotal: 100 kB\nMemFree: 101 kB\n");
    EXPECT_THROW(static_cast<void>(sysdiag::read_memory_stats(directory.path() / "meminfo")),
                 std::runtime_error);
}

TEST(ProcessTest, SortsNumericPidsAndReportsOptionalResidentMemory)
{
    TemporaryDirectory directory;
    directory.write("12/comm", "worker process\n");
    directory.write("12/status", "Name:\tworker process\nVmRSS:\t1536 kB\n");
    directory.write("3/comm", "short-lived\n");
    directory.write("not-a-pid/comm", "ignored\n");
    directory.write("8/status", "VmRSS:\t10 kB\n");

    const auto processes = sysdiag::list_processes(directory.path());

    ASSERT_EQ(processes.size(), 2U);
    EXPECT_EQ(processes[0].pid, 3);
    EXPECT_EQ(processes[0].name, "short-lived");
    EXPECT_FALSE(processes[0].resident_memory_kib.has_value());
    EXPECT_EQ(processes[1].pid, 12);
    EXPECT_EQ(processes[1].name, "worker process");
    ASSERT_TRUE(processes[1].resident_memory_kib.has_value());
    EXPECT_EQ(*processes[1].resident_memory_kib, 1536U);
}

TEST(ProcessTest, TreatsMalformedResidentMemoryAsUnavailable)
{
    TemporaryDirectory directory;
    directory.write("21/comm", "test-process\n");
    directory.write("21/status", "VmRSS:\tunknown kB\n");

    const auto processes = sysdiag::list_processes(directory.path());

    ASSERT_EQ(processes.size(), 1U);
    EXPECT_FALSE(processes[0].resident_memory_kib.has_value());
}

TEST(ProcessTest, RejectsAnUnavailableProcRoot)
{
    TemporaryDirectory directory;

    EXPECT_THROW(sysdiag::list_processes(directory.path() / "missing"),
                 std::runtime_error);
}

TEST(ProcIntegrationTest, ReadsLiveLinuxProcData)
{
    const auto cpu = sysdiag::read_cpu_times();
    const auto memory = sysdiag::read_memory_stats();
    const auto processes = sysdiag::list_processes();

    EXPECT_GT(cpu.user + cpu.system + cpu.idle, 0U);
    EXPECT_GT(memory.total_kib, 0U);
    EXPECT_LE(memory.available_kib, memory.total_kib);
    EXPECT_FALSE(processes.empty());
}