#include "commands.hpp"
#include "command_helpers.hpp"
#include "fbx_helpers.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <fbxsdk.h>

static std::string target_node_name;
static std::string from_node_name;
static int to_parent = -1;

static void run_make_root_command(const std::string &input_file, const std::string &target_node_name,
                                  const std::string &from_node_name, int to_parent,
                                  const std::string &output_file, const std::string &format, bool yes);

static void register_make_root_command(CLI::App &app, CommandContext &context)
{
    auto *command = app.add_subcommand("make-root", "Convert a node into a Skeleton root node");

    context.add_input_fbx_file_option(*command);
    context.add_output_fbx_file_option(*command);
    context.add_output_fbx_format_option(*command);
    context.add_output_overwrite_flag(*command);

    command->add_option("node,--to-node", target_node_name, "Name of the node that will become a Skeleton root");

    command->add_option("--from-node", from_node_name, "Name of an existing Skeleton root node to move from "
                                                          "(must be a child of the target node)");
    command->add_option("--to-parent", to_parent, "How many levels up from --from-node to create the new root");

    command->parse_complete_callback([&]()
                                     {
        bool has_to_node = !target_node_name.empty();
        bool has_from_node = !from_node_name.empty();
        bool has_to_parent = to_parent >= 0;

        if (!has_to_node && !(has_from_node && has_to_parent))
        {
            std::cerr << "Error: either --to-node <name> or --from-node <name> --to-parent <N> must be provided.\n";
            throw CLI::RuntimeError(EXIT_FAILURE);
        } });

    command->callback([&context]()
                      { run_make_root_command(context.input_file, target_node_name, from_node_name,
                                              to_parent, context.get_output_fbx_file(),
                                              context.output_fbx_format, context.yes); });
}

REGISTER_CLI_SETUP(register_make_root_command)

static void ensure_skeleton_limb_hierarchy(fbxsdk::FbxNode *node, fbxsdk::FbxNode *new_root)
{
    for (fbxsdk::FbxNode *cur = node; cur && cur != new_root; cur = cur->GetParent())
    {
        if (!cur->GetNodeAttribute() || cur->GetNull() || (cur->GetSkeleton() && cur->GetSkeleton()->GetSkeletonType() != fbxsdk::FbxSkeleton::eLimbNode))
        {
            fbxsdk::FbxSkeleton *skel = cur->GetSkeleton();
            if (!skel)
            {
                skel = fbxsdk::FbxSkeleton::Create(cur->GetScene()->GetFbxManager(), cur->GetName());
                fbxsdk::FbxNodeAttribute *old_attr = cur->SetNodeAttribute(skel);
                if (old_attr)
                    old_attr->Destroy();
            }

            skel->SetSkeletonType(fbxsdk::FbxSkeleton::eLimbNode);
        }
    }
}

static bool ancestor_is_skeleton_root(fbxsdk::FbxNode *node)
{
    for (fbxsdk::FbxNode *cur = node->GetParent(); cur; cur = cur->GetParent())
    {
        fbxsdk::FbxSkeleton *skel = cur->GetSkeleton();
        if (skel && skel->IsSkeletonRoot())
            return true;
    }
    return false;
}

static void convert_child_nulls_to_limbs(fbxsdk::FbxNode *node)
{
    if (!node)
        return;

    int child_count = node->GetChildCount();
    for (int i = 0; i < child_count; ++i)
    {
        fbxsdk::FbxNode *child = node->GetChild(i);

        // If this child is already a Skeleton root, convert it but don't recurse
        fbxsdk::FbxSkeleton *skel = child->GetSkeleton();
        if (skel && skel->IsSkeletonRoot())
        {
            skel->SetSkeletonType(fbxsdk::FbxSkeleton::eLimbNode);
            continue;
        }

        if (fbx_helpers::is_null_type(child))
        {
            if (!skel)
            {
                skel = fbxsdk::FbxSkeleton::Create(
                    child->GetScene()->GetFbxManager(),
                    child->GetName());
                fbxsdk::FbxNodeAttribute *old_attr = child->SetNodeAttribute(skel);
                if (old_attr)
                    old_attr->Destroy();
            }
            skel->SetSkeletonType(fbxsdk::FbxSkeleton::eLimbNode);
        }

        convert_child_nulls_to_limbs(child);
    }
}

static void run_make_root_command(const std::string &input_file, const std::string &target_node_name,
                                  const std::string &from_node_name, int to_parent,
                                  const std::string &output_file, const std::string &format, bool yes)
{
    if (!command_helpers::confirm_output_overwrite(output_file, yes))
        throw CLI::RuntimeError(EXIT_FAILURE);

    int input_file_format = -1;
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = command_helpers::import_scene(input_file, sdk_manager, &input_file_format);
    if (!scene)
        throw CLI::RuntimeError(EXIT_FAILURE);

    fbxsdk::FbxNode *scene_root = scene->GetRootNode();

    fbxsdk::FbxNode *target = nullptr;
    fbxsdk::FbxNode *from_node_ptr = nullptr;

    if (!target_node_name.empty())
    {
        // Mode: --to-node <name> (optionally with --from-node)
        target = scene_root->FindChild(target_node_name.c_str());
        if (!target)
        {
            std::cerr << "Error: node '" << target_node_name << "' not found.\n";
            throw CLI::RuntimeError(EXIT_FAILURE);
        }

        if (!from_node_name.empty())
        {
            from_node_ptr = target->FindChild(from_node_name.c_str());
            if (!from_node_ptr)
            {
                std::cerr << "Error: node '" << from_node_name << "' is not a descendant of '" << target_node_name << "'.\n";
                throw CLI::RuntimeError(EXIT_FAILURE);
            }

            if (!fbx_helpers::is_skeleton_root(from_node_ptr))
            {
                std::cerr << "Error: node '" << from_node_name << "' is not a Skeleton root node.\n";
                throw CLI::RuntimeError(EXIT_FAILURE);
            }
        }
    }
    else
    {
        // Mode: --from-node <name> --to-parent <N>
        fbxsdk::FbxNode *from = scene_root->FindChild(from_node_name.c_str());
        if (!from)
        {
            std::cerr << "Error: node '" << from_node_name << "' not found.\n";
            throw CLI::RuntimeError(EXIT_FAILURE);
        }

        if (!fbx_helpers::is_skeleton_root(from))
        {
            std::cerr << "Error: node '" << from_node_name << "' is not a Skeleton root node.\n";
            throw CLI::RuntimeError(EXIT_FAILURE);
        }

        // Walk up N levels from the from-node to find the target
        target = from;
        for (int i = 0; i < to_parent; ++i)
        {
            target = target->GetParent();
            if (!target || target == scene_root)
            {
                std::cerr << "Error: cannot walk up " << to_parent << " levels from '"
                          << from_node_name << "'.\n";
                throw CLI::RuntimeError(EXIT_FAILURE);
            }
        }

        from_node_ptr = from;
    }

    if (ancestor_is_skeleton_root(target))
    {
        std::cerr << "Error: an ancestor of '" << target->GetName() << "' is already a Skeleton root node.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    if (target->GetNodeAttribute() && !target->GetNull() && !target->GetSkeleton())
    {
        std::cerr << "'" << target->GetName() << "' is not a null node.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    fbxsdk::FbxSkeleton *skeleton_attr = target->GetSkeleton();
    if (!skeleton_attr)
    {
        skeleton_attr = fbxsdk::FbxSkeleton::Create(scene->GetFbxManager(), target->GetName());
        fbxsdk::FbxNodeAttribute *old_attr = target->SetNodeAttribute(skeleton_attr);
        if (old_attr)
            old_attr->Destroy();
    }

    skeleton_attr->SetSkeletonType(fbxsdk::FbxSkeleton::eLimbNode);

    convert_child_nulls_to_limbs(target);

    std::cout << "Converted '" << target->GetName() << "' to a Skeleton root node.\n";

    if (!command_helpers::export_scene(scene, output_file, format, input_file_format))
    {
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    std::cout << "Wrote '" << output_file << "'.\n";
}
