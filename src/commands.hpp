#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <CLI/CLI.hpp>

struct CommandContext
{
public:
    std::string get_output_fbx_file() const
    {
        return output_file.empty()
                   ? std::filesystem::path(input_file).filename().string()
                   : output_file;
    }

    void add_input_fbx_file_option(CLI::App &command)
    {
        command.add_option("input", input_file, "Input FBX file")
            ->required()
            ->check(CLI::ExistingFile);
    }

    void add_output_fbx_file_option(CLI::App &command)
    {
        command.add_option("-o,--output", output_file, "Output FBX file path (default: <input filename> at CWD)");
    }

    void add_output_fbx_format_option(CLI::App &command)
    {
        command.add_option("-f,--format", output_fbx_format, "Output format: auto, ascii, or binary (default: auto)")
            ->default_val("auto")
            ->check(CLI::IsMember({"auto", "ascii", "binary"}));
    }

    void add_output_overwrite_flag(CLI::App &command)
    {
        command.add_flag("-y,--yes", yes, "Auto-confirm overwrite of existing output file");
    }

public:
    std::string input_file;
    std::string output_file;
    std::string output_fbx_format = "auto";
    bool yes = false;
};

class CommandRegistry
{
public:
    using SetupFn = std::function<void(CLI::App &, CommandContext &)>;

    static CommandRegistry &instance()
    {
        static CommandRegistry registry;
        return registry;
    }

    void add(SetupFn fn)
    {
        registrars_.push_back(std::move(fn));
    }

    void setup_all(CLI::App &app, CommandContext &context) const
    {
        for (const auto &fn : registrars_)
        {
            fn(app, context);
        }
    }

private:
    CommandRegistry() = default;
    std::vector<SetupFn> registrars_;
};

#define REGISTER_CLI_SETUP(fn)                       \
    namespace                                        \
    {                                                \
        const bool _cli_cmd_setup_##fn = []() {                 \
            ::CommandRegistry::instance().add(fn);                   \
            return true; }(); \
    }