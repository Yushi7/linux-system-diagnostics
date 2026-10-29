#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace sysdiag
{

    struct Options
    {
        bool show_cpu{true};
        bool show_memory{true};
        bool show_processes{true};
        std::chrono::milliseconds sample_interval{250};
        std::filesystem::path proc_root{"/proc"};
        std::size_t max_processes{};
    };

    [[nodiscard]] Options parse_arguments(int argc, char *argv[]);
    [[nodiscard]] std::string help_message(std::string_view program_name);

} // namespace sysdiag