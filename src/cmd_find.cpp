#include "commands.hpp"
#include "command_helpers.hpp"
#include "fbx_helpers.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <fbxsdk.h>

static bool tree = false;

static void run_find_command(const std::string &input_file, bool print_tree);

static void register_find_command(CLI::App &app, CommandContext &context)
{
    auto *command = app.add_subcommand("find", "Find nodes marked as the root Skeleton node");

    context.add_input_fbx_file_option(*command);

    command->add_flag("-t,--tree", tree, "Show the full ancestor path from root to the node itself");
    command->callback([&context]()
                      { run_find_command(context.input_file, tree); });
}

REGISTER_CLI_SETUP(register_find_command)

static void find_skeleton_roots(fbxsdk::FbxNode *node,
                                std::vector<fbxsdk::FbxNode *> &results)
{
    if (!node)
        return;

    if (node->GetSkeleton() && node->GetSkeleton()->IsSkeletonRoot())
    {
        results.push_back(node);
    }

    int child_count = node->GetChildCount();
    for (int i = 0; i < child_count; ++i)
        find_skeleton_roots(node->GetChild(i), results);
}

static void run_find_command(const std::string &input_file, bool print_tree)
{
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = command_helpers::import_scene(input_file, sdk_manager);
    if (!scene)
        throw CLI::RuntimeError(EXIT_FAILURE);

    std::vector<fbxsdk::FbxNode *> root_bones;
    find_skeleton_roots(scene->GetRootNode(), root_bones);

    for (auto *node : root_bones)
    {
        if (print_tree)
        {
            std::vector<std::string> chain;
            for (fbxsdk::FbxNode *cur = node; cur; cur = cur->GetParent())
            {
                if (cur == scene->GetRootNode())
                    continue;
                chain.emplace_back(cur->GetName());
            }

            std::reverse(chain.begin(), chain.end());
            std::cout << command_helpers::build_hierarchy_string(chain) << '\n';
        }
        else
        {
            std::cout << node->GetName() << '\n';
        }
    }
}
