#include "sysdiag/cli.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace sysdiag
{
    namespace
    {

        std::uint64_t parse_positive_integer(std::string_view value,
                                             std::string_view option,
                                             std::uint64_t maximum)
        {
            std::uint64_t parsed{};
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), parsed);
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

    } // namespace

    Options parse_arguments(int argc, char *argv[])
    {
        Options options;
        bool section_selected{};

        const auto select_section = [&options, &section_selected]()
        {
            if (!section_selected)
            {
                options.show_cpu = false;
                options.show_memory = false;
                options.show_processes = false;
                section_selected = true;
            }
        };

        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument{argv[index]};
            if (argument == "--cpu")
            {
                select_section();
                options.show_cpu = true;
            }
            else if (argument == "--memory")
            {
                select_section();
                options.show_memory = true;
            }
            else if (argument == "--processes")
            {
                select_section();
                options.show_processes = true;
            }
            else if (argument == "--sample-interval-ms" ||
                     argument == "--proc-root" ||
                     argument == "--max-processes")
            {
                if (index + 1 >= argc)
                {
                    throw std::invalid_argument("Missing value for " +
                                                std::string(argument));
                }

                const std::string_view value{argv[++index]};
                if (argument == "--sample-interval-ms")
                {
                    const auto milliseconds =
                        parse_positive_integer(value, argument, 60000);
                    options.sample_interval =
                        std::chrono::milliseconds{milliseconds};
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
                    options.max_processes = static_cast<std::size_t>(
                        parse_positive_integer(
                            value, argument,
                            static_cast<std::uint64_t>(
                                std::numeric_limits<std::size_t>::max())));
                }
            }
            else
            {
                throw std::invalid_argument("Unknown option: " +
                                            std::string(argument));
            }
        }

        return options;
    }

    std::string help_message(std::string_view program_name)
    {
        return "Usage: " + std::string(program_name) + " [options]\n"
                                                       "Display Linux CPU, memory, and process diagnostics.\n\n"
                                                       "Options:\n"
                                                       "  --cpu                    Show CPU usage and time breakdown\n"
                                                       "  --memory                 Show memory totals\n"
                                                       "  --processes              Show running processes\n"
                                                       "  --sample-interval-ms N   CPU sampling interval (1-60000, default 250)\n"
                                                       "  --proc-root PATH         Read Linux procfs data from PATH (default /proc)\n"
                                                       "  --max-processes N        Limit displayed processes (default: all)\n"
                                                       "  -h, --help               Show this help\n\n"
                                                       "By default, all diagnostic sections are shown. Section options can be combined.\n";
    }

} // namespace sysdiag