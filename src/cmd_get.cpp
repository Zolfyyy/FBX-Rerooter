#include "commands.hpp"
#include "command_helpers.hpp"
#include "fbx_helpers.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <fbxsdk.h>

static std::string hierarchy;

static void run_get_command(const std::string &input_file, const std::string &hierarchy);

static void register_get_command(CLI::App &app, CommandContext &context)
{
    auto *command = app.add_subcommand("get", "Find a node by its full hierarchy path");

    context.add_input_fbx_file_option(*command);

    command->add_option("hierarchy", hierarchy, "Comma-separated hierarchy path from the scene root")
        ->required();
    command->callback([&context]()
                      { run_get_command(context.input_file, hierarchy); });
}

REGISTER_CLI_SETUP(register_get_command)

static void run_get_command(const std::string &input_file, const std::string &hierarchy)
{
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = command_helpers::import_scene(input_file, sdk_manager);
    if (!scene)
        throw CLI::RuntimeError(EXIT_FAILURE);

    fbxsdk::FbxNode *node = command_helpers::find_node_by_hierarchy_string(scene->GetRootNode(), hierarchy);
    if (!node)
    {
        std::cerr << "Error: hierarchy '" << hierarchy << "' not found.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    std::cout << hierarchy << '\n';
}
