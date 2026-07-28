#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <CLI/CLI.hpp>
#include <fbxsdk.h>
#include "fbx_helpers.hpp"

namespace fs = std::filesystem;

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

static int run_find_command(const std::string &input_file, bool print_tree)
{
    fbxsdk::FbxManager *sdk_manager = fbxsdk::FbxManager::Create();
    if (!sdk_manager)
    {
        std::cerr << "Fatal: failed to create FBX SDK manager.\n";
        return EXIT_FAILURE;
    }

    fbxsdk::FbxIOSettings *ios = fbxsdk::FbxIOSettings::Create(sdk_manager, IOSROOT);
    sdk_manager->SetIOSettings(ios);

    fbxsdk::FbxImporter *importer = fbxsdk::FbxImporter::Create(sdk_manager, "");
    if (!importer->Initialize(input_file.c_str(), -1, sdk_manager->GetIOSettings()))
    {
        std::cerr << "Fatal: could not open '" << input_file << "': "
                  << importer->GetStatus().GetErrorString() << '\n';
        importer->Destroy();
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    fbxsdk::FbxScene *scene = fbxsdk::FbxScene::Create(sdk_manager, "");
    if (!importer->Import(scene))
    {
        std::cerr << "Fatal: import failed for '" << input_file << "': "
                  << importer->GetStatus().GetErrorString() << '\n';
        importer->Destroy();
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    importer->Destroy();

    std::vector<fbxsdk::FbxNode *> root_bones;
    fbx_helpers::find_skeleton_roots(scene->GetRootNode(), root_bones);

    for (auto *node : root_bones)
    {
        if (print_tree)
        {
            std::vector<const char *> chain;
            for (fbxsdk::FbxNode *cur = node; cur; cur = cur->GetParent())
            {
                if (cur == scene->GetRootNode())
                    continue;
                chain.push_back(cur->GetName());
            }

            for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i)
                std::cout << (i == static_cast<int>(chain.size()) - 1 ? "" : ",") << chain[i];
            std::cout << '\n';
        }
        else
        {
            std::cout << node->GetName() << '\n';
        }
    }

    sdk_manager->Destroy();
    return EXIT_SUCCESS;
}

static int run_make_root_command(const std::string &input_file, const std::string &target_node_name,
                                 const std::string &from_node_name, int to_parent,
                                 const std::string &output_file, const std::string &format)
{
    fbxsdk::FbxManager *sdk_manager = fbxsdk::FbxManager::Create();
    if (!sdk_manager)
    {
        std::cerr << "Fatal: failed to create FBX SDK manager." << '\n';
        return EXIT_FAILURE;
    }

    fbxsdk::FbxIOSettings *ios = fbxsdk::FbxIOSettings::Create(sdk_manager, IOSROOT);
    sdk_manager->SetIOSettings(ios);

    fbxsdk::FbxImporter *importer = fbxsdk::FbxImporter::Create(sdk_manager, "");
    if (!importer->Initialize(input_file.c_str(), -1, sdk_manager->GetIOSettings()))
    {
        std::cerr << "Fatal: could not open '" << input_file << "': "
                  << importer->GetStatus().GetErrorString() << '\n';
        importer->Destroy();
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    // Set format
    bool ascii = format == "ascii";
    if (format == "ascii")
    {
        ascii = true;
    }
    else if (format == "auto")
    {
        ascii = importer->GetFileFormat() == 1;
    }

    fbxsdk::FbxScene *scene = fbxsdk::FbxScene::Create(sdk_manager, "");
    if (!importer->Import(scene))
    {
        std::cerr << "Fatal: import failed for '" << input_file << "': "
                  << importer->GetStatus().GetErrorString() << '\n';
        importer->Destroy();
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    importer->Destroy();

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
            sdk_manager->Destroy();
            return EXIT_FAILURE;
        }

        if (!from_node_name.empty())
        {
            from_node_ptr = target->FindChild(from_node_name.c_str());
            if (!from_node_ptr)
            {
                std::cerr << "Error: node '" << from_node_name << "' is not a descendant of '" << target_node_name << "'.\n";
                sdk_manager->Destroy();
                return EXIT_FAILURE;
            }

            if (!fbx_helpers::is_skeleton_root(from_node_ptr))
            {
                std::cerr << "Error: node '" << from_node_name << "' is not a Skeleton root node.\n";
                sdk_manager->Destroy();
                return EXIT_FAILURE;
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
            sdk_manager->Destroy();
            return EXIT_FAILURE;
        }

        if (!fbx_helpers::is_skeleton_root(from))
        {
            std::cerr << "Error: node '" << from_node_name << "' is not a Skeleton root node.\n";
            sdk_manager->Destroy();
            return EXIT_FAILURE;
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
                sdk_manager->Destroy();
                return EXIT_FAILURE;
            }
        }

        from_node_ptr = from;
    }

    if (ancestor_is_skeleton_root(target))
    {
        std::cerr << "Error: an ancestor of '" << target->GetName() << "' is already a Skeleton root node.\n";
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    if (target->GetNodeAttribute() && !target->GetNull() && !target->GetSkeleton())
    {
        std::cerr << "'" << target->GetName() << "' is not a null node.\n";
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    fbxsdk::FbxSkeleton *skeleton_attr = target->GetSkeleton();
    if (!skeleton_attr)
    {
        skeleton_attr = fbxsdk::FbxSkeleton::Create(sdk_manager, target->GetName());
        fbxsdk::FbxNodeAttribute *old_attr = target->SetNodeAttribute(skeleton_attr);
        if (old_attr)
            old_attr->Destroy();
    }

    skeleton_attr->SetSkeletonType(fbxsdk::FbxSkeleton::eLimbNode);

    convert_child_nulls_to_limbs(target);

    std::cout << "Converted '" << target->GetName() << "' to a Skeleton root node.\n";

    fbxsdk::FbxExporter *exporter = fbxsdk::FbxExporter::Create(sdk_manager, "");

    if (!exporter->Initialize(output_file.c_str(), ascii ? 1 : 0, ios))
    {
        std::cerr << "Fatal: could not initialize exporter for '" << output_file << "': "
                  << exporter->GetStatus().GetErrorString() << '\n';
        exporter->Destroy();
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    if (!exporter->Export(scene))
    {
        std::cerr << "Fatal: export failed for '" << output_file << "': "
                  << exporter->GetStatus().GetErrorString() << '\n';
        exporter->Destroy();
        sdk_manager->Destroy();
        return EXIT_FAILURE;
    }

    exporter->Destroy();
    std::cout << "Wrote '" << output_file << "'.\n";
    sdk_manager->Destroy();
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    CLI::App app{"FBX armature re-rooting utility"};

    std::string opt_input_file;
    std::string opt_node_name;
    std::string opt_output_file;
    std::string opt_output_format = "auto";
    bool opt_tree = false;

    std::string opt_from_node;
    int opt_to_parent = -1;
    bool opt_yes = false;

    auto *find_command = app.add_subcommand("find", "Find nodes marked as the root Skeleton node");
    find_command->add_option("input", opt_input_file, "Input FBX file")
        ->required()
        ->check(CLI::ExistingFile);
    find_command->add_flag("-t,--tree", opt_tree, "Show the full ancestor path from root to the node itself");

    auto *version_command = app.add_subcommand("version", "Display the program version");

    auto *mkroot_command = app.add_subcommand("make-root", "Convert a node into a Skeleton root node");
    mkroot_command->add_option("input", opt_input_file, "Input FBX file")
        ->required()
        ->check(CLI::ExistingFile);
    mkroot_command->add_option("node,--to-node", opt_node_name, "Name of the node that will become a Skeleton root");
    mkroot_command->add_option("-o,--output", opt_output_file, "Output FBX file path (default: <input filename> at CWD)");
    mkroot_command->add_option("-f,--format", opt_output_format, "Output format: auto, ascii, or binary (default: auto)")
        ->default_val("auto")
        ->check(CLI::IsMember({"auto", "ascii", "binary"}));
    mkroot_command->add_option("--from-node", opt_from_node, "Name of an existing Skeleton root node to move from "
                                                             "(must be a child of the target node)");
    mkroot_command->add_option("--to-parent", opt_to_parent, "How many levels up from --from-node to create the new root");
    mkroot_command->add_flag("-y,--yes", opt_yes, "Auto-confirm overwrite of existing output file");

    mkroot_command->parse_complete_callback([&]()
                                            {
        bool has_to_node = !opt_node_name.empty();
        bool has_from_node = !opt_from_node.empty();
        bool has_to_parent = opt_to_parent >= 0;

        if (!has_to_node && !(has_from_node && has_to_parent))
        {
            std::cerr << "Error: either --to-node <name> or --from-node <name> --to-parent <N> must be provided.\n";
            std::exit(EXIT_FAILURE);
        } });

    app.require_subcommand(1);

    CLI11_PARSE(app, argc, argv);

    if (app.got_subcommand(version_command))
    {
        std::cout << PROJECT_VERSION << '\n';
        return EXIT_SUCCESS;
    }
    else if (app.got_subcommand(find_command))
    {
        return run_find_command(opt_input_file, opt_tree);
    }
    else if (app.got_subcommand(mkroot_command))
    {
        if (opt_output_file.empty())
            opt_output_file = fs::path(opt_input_file).filename().string();

        if (!opt_yes && fs::exists(opt_output_file))
        {
            std::cout << "File '" << opt_output_file << "' already exists." << '\n'
                      << "Overwrite? (y/N): ";
            std::string response;
            std::getline(std::cin, response);
            if (response != "y" && response != "Y")
            {
                std::cout << "Aborted.\n";
                return EXIT_FAILURE;
            }
        }

        return run_make_root_command(opt_input_file, opt_node_name, opt_from_node, opt_to_parent, opt_output_file, opt_output_format);
    }

    return EXIT_SUCCESS;
}