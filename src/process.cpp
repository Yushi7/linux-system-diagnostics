#include "sysdiag/process.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace sysdiag
{
    namespace
    {

        std::optional<int> parse_pid(const std::string &name)
        {
            int pid{};
            const auto result =
                std::from_chars(name.data(), name.data() + name.size(), pid);
            if (result.ec != std::errc{} ||
                result.ptr != name.data() + name.size() || pid <= 0)
            {
                return std::nullopt;
            }
            return pid;
        }

        std::optional<std::string> read_process_name(
            const std::filesystem::path &process_path)
        {
            std::ifstream input(process_path / "comm");
            std::string name;
            if (!input || !std::getline(input, name))
            {
                return std::nullopt;
            }
            if (!name.empty() && name.back() == '\r')
            {
                name.pop_back();
            }
            if (name.empty())
            {
                return std::nullopt;
            }
            return name;
        }

        std::optional<std::uint64_t> read_resident_memory_kib(
            const std::filesystem::path &process_path)
        {
            std::ifstream input(process_path / "status");
            std::string line;
            while (input && std::getline(input, line))
            {
                std::istringstream row(line);
                std::string key;
                if (!(row >> key) || key != "VmRSS:")
                {
                    continue;
                }

                std::string value_text;
                std::string unit;
                if (!(row >> value_text >> unit) || unit != "kB")
                {
                    return std::nullopt;
                }

                std::uint64_t value{};
                const auto result = std::from_chars(
                    value_text.data(), value_text.data() + value_text.size(), value);
                if (result.ec != std::errc{} ||
                    result.ptr != value_text.data() + value_text.size())
                {
                    return std::nullopt;
                }
                return value;
            }
            return std::nullopt;
        }

    } // namespace

    std::vector<ProcessInfo> list_processes(
        const std::filesystem::path &proc_root)
    {
        std::error_code error;
        std::filesystem::directory_iterator iterator(
            proc_root, std::filesystem::directory_options::skip_permission_denied,
            error);
        if (error)
        {
            throw std::runtime_error("Unable to enumerate " + proc_root.string() +
                                     ": " + error.message());
        }

        std::vector<ProcessInfo> processes;
        const std::filesystem::directory_iterator end;
        while (iterator != end)
        {
            const auto process_path = iterator->path();
            const auto pid = parse_pid(process_path.filename().string());
            if (pid)
            {
                const auto name = read_process_name(process_path);
                if (name)
                {
                    processes.push_back(
                        {*pid, *name, read_resident_memory_kib(process_path)});
                }
            }

            iterator.increment(error);
            if (error)
            {
                throw std::runtime_error("Unable to continue enumerating " +
                                         proc_root.string() + ": " +
                                         error.message());
            }
        }

        std::sort(processes.begin(), processes.end(),
                  [](const ProcessInfo &left, const ProcessInfo &right)
                  { return left.pid < right.pid; });
        return processes;
    }

} // namespace sysdiag