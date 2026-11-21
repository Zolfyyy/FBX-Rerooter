#include "commands.hpp"
#include "command_helpers.hpp"
#include "fbx_helpers.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <fbxsdk.h>

static std::string hierarchy;
static bool create_parents = false;

static void run_create_command(const std::string &input_file, const std::string &hierarchy,
                               const std::string &output_file, const std::string &format, bool yes, bool parents);

static void register_create_command(CLI::App &app, CommandContext &context)
{
    auto *command = app.add_subcommand("create", "Create a Null node at a hierarchy path if it does not exist");

    context.add_input_fbx_file_option(*command);
    context.add_output_fbx_file_option(*command);
    context.add_output_fbx_format_option(*command);
    context.add_output_overwrite_flag(*command);

    command->add_option("hierarchy", hierarchy, "Comma-separated hierarchy path from the scene root")
        ->required();
    command->add_flag("-p", create_parents, "Create any missing parent nodes in the hierarchy");

    command->callback([&context]()
                      { run_create_command(context.input_file, hierarchy,
                                           context.get_output_fbx_file(), context.output_fbx_format,
                                           context.yes, create_parents); });
}

REGISTER_CLI_SETUP(register_create_command)

static void run_create_command(const std::string &input_file, const std::string &hierarchy,
                               const std::string &output_file, const std::string &format, bool yes, bool create_parents)
{
    if (!command_helpers::confirm_output_overwrite(output_file, yes))
        throw CLI::RuntimeError(EXIT_FAILURE);

    std::vector<std::string> node_names;
    if (!command_helpers::parse_hierarchy_string(hierarchy, node_names))
    {
        std::cerr << "Error: hierarchy must contain non-empty comma-separated node names.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    const std::string node_name = node_names.back();
    node_names.pop_back();
    const std::string parent_hierarchy = command_helpers::build_hierarchy_string(node_names);

    int input_file_format = -1;
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = command_helpers::import_scene(input_file, sdk_manager, &input_file_format);
    if (!scene)
        throw CLI::RuntimeError(EXIT_FAILURE);

    fbxsdk::FbxNode *scene_root = scene->GetRootNode();
    if (command_helpers::find_node_by_hierarchy_string(scene_root, hierarchy))
    {
        std::cout << "Node '" << hierarchy << "' already exists; no changes made.\n";
        return;
    }

    fbxsdk::FbxNode *parent = scene_root;
    for (const std::string &ancestor_name : node_names)
    {
        fbxsdk::FbxNode *ancestor = nullptr;
        for (int i = 0; i < parent->GetChildCount(); ++i)
        {
            fbxsdk::FbxNode *child = parent->GetChild(i);
            if (ancestor_name == child->GetName())
            {
                ancestor = child;
                break;
            }
        }

        if (!ancestor)
        {
            if (!create_parents)
            {
                std::cerr << "Error: parent hierarchy '" << parent_hierarchy << "' not found. Use -p to create missing parent nodes.\n";
                throw CLI::RuntimeError(EXIT_FAILURE);
            }

            ancestor = fbxsdk::FbxNode::Create(scene, ancestor_name.c_str());
            fbxsdk::FbxNull *null_attribute = fbxsdk::FbxNull::Create(scene, ancestor_name.c_str());
            ancestor->SetNodeAttribute(null_attribute);
            parent->AddChild(ancestor);
        }

        parent = ancestor;
    }

    fbxsdk::FbxNode *new_node = fbxsdk::FbxNode::Create(scene, node_name.c_str());
    fbxsdk::FbxNull *null_attribute = fbxsdk::FbxNull::Create(scene, node_name.c_str());
    new_node->SetNodeAttribute(null_attribute);
    parent->AddChild(new_node);

    if (!command_helpers::export_scene(scene, output_file, format, input_file_format))
    {
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    std::cout << "Created '" << hierarchy << "' as a Null node.\n"
              << "Wrote '" << output_file << "'.\n";
}
