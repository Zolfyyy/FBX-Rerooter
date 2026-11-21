#include "commands.hpp"
#include "command_helpers.hpp"
#include "fbx_helpers.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <fbxsdk.h>

static std::string source_hierarchy;
static std::string destination_hierarchy;

static void run_move_command(const std::string &input_file, const std::string &source_hierarchy,
                             const std::string &destination_hierarchy, const std::string &output_file,
                             const std::string &format, bool yes);

static void register_move_command(CLI::App &app, CommandContext &context)
{
    auto *command = app.add_subcommand("move", "Move a node to another hierarchy path");

    context.add_input_fbx_file_option(*command);
    context.add_output_fbx_file_option(*command);
    context.add_output_fbx_format_option(*command);
    context.add_output_overwrite_flag(*command);

    command->add_option("source", source_hierarchy, "Comma-separated hierarchy path of the node to move")
        ->required();
    command->add_option("destination", destination_hierarchy, "Full destination hierarchy path, including the node name")
        ->required();

    command->callback([&context]()
                      { run_move_command(context.input_file, source_hierarchy,
                                         destination_hierarchy,
                                         context.get_output_fbx_file(), context.output_fbx_format, context.yes); });
}

REGISTER_CLI_SETUP(register_move_command)

static void run_move_command(const std::string &input_file, const std::string &source_hierarchy,
                             const std::string &destination_hierarchy, const std::string &output_file,
                             const std::string &format, bool yes)
{
    std::vector<std::string> source_names;
    std::vector<std::string> destination_names;
    if (!command_helpers::parse_hierarchy_string(source_hierarchy, source_names) ||
        !command_helpers::parse_hierarchy_string(destination_hierarchy, destination_names))
    {
        std::cerr << "Error: source and destination hierarchies must contain non-empty comma-separated node names.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    if (!command_helpers::confirm_output_overwrite(output_file, yes))
        throw CLI::RuntimeError(EXIT_FAILURE);

    int input_file_format = -1;
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = command_helpers::import_scene(input_file, sdk_manager, &input_file_format);
    if (!scene)
        throw CLI::RuntimeError(EXIT_FAILURE);

    fbxsdk::FbxNode *scene_root = scene->GetRootNode();
    fbxsdk::FbxNode *source = command_helpers::find_node_by_hierarchy_string(scene_root, source_hierarchy);
    if (!source)
    {
        std::cerr << "Error: source hierarchy '" << source_hierarchy << "' not found.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    const std::string destination_name = destination_names.back();
    destination_names.pop_back();
    const std::string destination_parent_hierarchy = command_helpers::build_hierarchy_string(destination_names);
    fbxsdk::FbxNode *destination_parent = destination_names.empty()
                                              ? scene_root
                                              : command_helpers::find_node_by_hierarchy_string(scene_root, destination_parent_hierarchy);
    if (!destination_parent)
    {
        std::cerr << "Error: destination parent hierarchy '" << destination_parent_hierarchy << "' not found.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    for (fbxsdk::FbxNode *ancestor = destination_parent; ancestor; ancestor = ancestor->GetParent())
    {
        if (ancestor == source)
        {
            std::cerr << "Error: destination parent cannot be the source node or one of its descendants.\n";
            throw CLI::RuntimeError(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < destination_parent->GetChildCount(); ++i)
    {
        fbxsdk::FbxNode *child = destination_parent->GetChild(i);
        if (child != source && destination_name == child->GetName())
        {
            std::cerr << "Error: destination node '" << destination_hierarchy << "' already exists.\n";
            throw CLI::RuntimeError(EXIT_FAILURE);
        }
    }

    if (source->GetParent() == destination_parent && destination_name == source->GetName())
    {
        std::cout << "Node '" << source_hierarchy << "' is already at '" << destination_hierarchy << "'; no changes made.\n";
        return;
    }

    fbxsdk::FbxNode *source_parent = source->GetParent();
    if (!source_parent->RemoveChild(source))
    {
        std::cerr << "Error: could not detach source node '" << source_hierarchy << "'.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }
    source->SetName(destination_name.c_str());
    if (!destination_parent->AddChild(source))
    {
        std::cerr << "Error: could not attach node to destination parent '" << destination_parent_hierarchy << "'.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    if (!command_helpers::export_scene(scene, output_file, format, input_file_format))
        throw CLI::RuntimeError(EXIT_FAILURE);

    std::cout << "Moved '" << source_hierarchy << "' to '" << destination_hierarchy << "'.\n"
              << "Wrote '" << output_file << "'.\n";
}
