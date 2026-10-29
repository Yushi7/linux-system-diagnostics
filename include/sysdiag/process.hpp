#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sysdiag
{

    struct ProcessInfo
    {
        int pid{};
        std::string name;
        std::optional<std::uint64_t> resident_memory_kib;
    };

    [[nodiscard]] std::vector<ProcessInfo> list_processes(
        const std::filesystem::path &proc_root = "/proc");

} // namespace sysdiag