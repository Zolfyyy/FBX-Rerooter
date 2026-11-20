#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <CLI/CLI.hpp>
#include <fbxsdk.h>
#include "fbx_helpers.hpp"

namespace fs = std::filesystem;

#pragma region Helpers

static bool should_export_ascii(const std::string &format, fbxsdk::FbxManager *sdk_manager,
                                int input_file_format)
{
    if (format == "ascii")
        return true;
    if (format == "auto")
        return fbx_helpers::get_ascii_reader_index(sdk_manager->GetIOPluginRegistry()) == input_file_format;
    return false;
}

static bool confirm_output_overwrite(const std::string &output_file, bool yes)
{
    if (yes || !fs::exists(output_file))
        return true;

    std::cout << "File '" << output_file << "' already exists." << '\n'
              << "Overwrite? (y/N): ";
    std::string response;
    std::getline(std::cin, response);
    if (response == "y" || response == "Y")
        return true;

    std::cout << "Aborted.\n";
    return false;
}

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

static bool parse_hierarchy_string(const std::string &hierarchy, std::vector<std::string> &node_names)
{
    node_names.clear();
    std::size_t start = 0;
    while (start <= hierarchy.size())
    {
        const std::size_t separator = hierarchy.find(',', start);
        const std::size_t end = separator == std::string::npos ? hierarchy.size() : separator;
        if (start == end)
        {
            node_names.clear();
            return false;
        }

        node_names.push_back(hierarchy.substr(start, end - start));
        if (separator == std::string::npos)
            return true;
        start = separator + 1;
    }

    return false;
}

static std::string build_hierarchy_string(const std::vector<std::string> &node_names)
{
    std::string hierarchy;
    for (const std::string &node_name : node_names)
    {
        if (!hierarchy.empty())
            hierarchy += ',';
        hierarchy += node_name;
    }
    return hierarchy;
}

static fbxsdk::FbxNode *find_node_by_hierarchy_string(fbxsdk::FbxNode *root,
                                                      const std::string &hierarchy)
{
    std::vector<std::string> node_names;
    if (!root || !parse_hierarchy_string(hierarchy, node_names))
        return nullptr;

    fbxsdk::FbxNode *node = root;
    for (const std::string &node_name : node_names)
    {
        fbxsdk::FbxNode *child_match = nullptr;
        for (int i = 0; i < node->GetChildCount(); ++i)
        {
            fbxsdk::FbxNode *child = node->GetChild(i);
            if (node_name == child->GetName())
            {
                child_match = child;
                break;
            }
        }

        if (!child_match)
            return nullptr;
        node = child_match;
    }

    return node;
}

static fbxsdk::FbxScene *import_scene(const std::string &input_file,
                                      fbx_helpers::FbxManagerPtr &sdk_manager,
                                      int *input_file_format = nullptr)
{
    sdk_manager.reset(fbxsdk::FbxManager::Create());
    if (!sdk_manager)
    {
        std::cerr << "Fatal: failed to create FBX SDK manager.\n";
        return nullptr;
    }

    sdk_manager->SetIOSettings(fbxsdk::FbxIOSettings::Create(sdk_manager.get(), IOSROOT));

    fbxsdk::FbxImporter *importer = fbxsdk::FbxImporter::Create(sdk_manager.get(), "");
    if (!importer->Initialize(input_file.c_str(), -1, sdk_manager->GetIOSettings()))
    {
        std::cerr << "Fatal: could not open '" << input_file << "': "
                  << importer->GetStatus().GetErrorString() << '\n';
        importer->Destroy();
        return nullptr;
    }

    if (input_file_format)
        *input_file_format = importer->GetFileFormat();

    fbxsdk::FbxScene *scene = fbxsdk::FbxScene::Create(sdk_manager.get(), "");
    if (!importer->Import(scene))
    {
        std::cerr << "Fatal: import failed for '" << input_file << "': "
                  << importer->GetStatus().GetErrorString() << '\n';
        importer->Destroy();
        return nullptr;
    }

    importer->Destroy();
    return scene;
}

static bool export_scene(fbxsdk::FbxScene *scene, fbxsdk::FbxManager *sdk_manager,
                         const std::string &output_file, int file_format)
{
    fbxsdk::FbxExporter *exporter = fbxsdk::FbxExporter::Create(sdk_manager, "");
    if (!exporter->Initialize(output_file.c_str(), file_format, sdk_manager->GetIOSettings()))
    {
        std::cerr << "Fatal: could not initialize exporter for '" << output_file << "': "
                  << exporter->GetStatus().GetErrorString() << '\n';
        exporter->Destroy();
        return false;
    }

    if (!exporter->Export(scene))
    {
        std::cerr << "Fatal: export failed for '" << output_file << "': "
                  << exporter->GetStatus().GetErrorString() << '\n';
        exporter->Destroy();
        return false;
    }

    exporter->Destroy();
    return true;
}

#pragma endregion

#pragma region Commands

static int run_find_command(const std::string &input_file, bool print_tree)
{
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = import_scene(input_file, sdk_manager);
    if (!scene)
        return EXIT_FAILURE;

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
            std::cout << build_hierarchy_string(chain) << '\n';
        }
        else
        {
            std::cout << node->GetName() << '\n';
        }
    }

    return EXIT_SUCCESS;
}

static int run_get_command(const std::string &input_file, const std::string &hierarchy)
{
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = import_scene(input_file, sdk_manager);
    if (!scene)
        return EXIT_FAILURE;

    fbxsdk::FbxNode *node = find_node_by_hierarchy_string(scene->GetRootNode(), hierarchy);
    if (!node)
    {
        std::cerr << "Error: hierarchy '" << hierarchy << "' not found.\n";
        return EXIT_FAILURE;
    }

    std::cout << hierarchy << '\n';
    return EXIT_SUCCESS;
}

static int run_create_command(const std::string &input_file, const std::string &hierarchy,
                              const std::string &output_file, const std::string &format, bool yes, bool parents)
{
    if (!confirm_output_overwrite(output_file, yes))
        return EXIT_FAILURE;

    std::vector<std::string> node_names;
    if (!parse_hierarchy_string(hierarchy, node_names))
    {
        std::cerr << "Error: hierarchy must contain non-empty comma-separated node names.\n";
        return EXIT_FAILURE;
    }

    const std::string node_name = node_names.back();
    node_names.pop_back();
    const std::string parent_hierarchy = build_hierarchy_string(node_names);

    int input_file_format = -1;
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = import_scene(input_file, sdk_manager, &input_file_format);
    if (!scene)
        return EXIT_FAILURE;

    const bool ascii = should_export_ascii(format, sdk_manager.get(), input_file_format);

    fbxsdk::FbxNode *scene_root = scene->GetRootNode();
    if (find_node_by_hierarchy_string(scene_root, hierarchy))
    {
        std::cout << "Node '" << hierarchy << "' already exists; no changes made.\n";
        return EXIT_SUCCESS;
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
            if (!parents)
            {
                std::cerr << "Error: parent hierarchy '" << parent_hierarchy << "' not found. Use -p to create missing parent nodes.\n";
                return EXIT_FAILURE;
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

    const int file_format = ascii
                                ? fbx_helpers::get_ascii_writer_index(sdk_manager->GetIOPluginRegistry())
                                : fbx_helpers::get_binary_writer_index(sdk_manager->GetIOPluginRegistry());
    if (!export_scene(scene, sdk_manager.get(), output_file, file_format))
    {
        return EXIT_FAILURE;
    }

    std::cout << "Created '" << hierarchy << "' as a Null node.\n"
              << "Wrote '" << output_file << "'.\n";
    return EXIT_SUCCESS;
}

static int run_make_root_command(const std::string &input_file, const std::string &target_node_name,
                                 const std::string &from_node_name, int to_parent,
                                 const std::string &output_file, const std::string &format, bool yes)
{
    if (!confirm_output_overwrite(output_file, yes))
        return EXIT_FAILURE;

    int input_file_format = -1;
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = import_scene(input_file, sdk_manager, &input_file_format);
    if (!scene)
        return EXIT_FAILURE;

    const bool ascii = should_export_ascii(format, sdk_manager.get(), input_file_format);

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
            return EXIT_FAILURE;
        }

        if (!from_node_name.empty())
        {
            from_node_ptr = target->FindChild(from_node_name.c_str());
            if (!from_node_ptr)
            {
                std::cerr << "Error: node '" << from_node_name << "' is not a descendant of '" << target_node_name << "'.\n";
                return EXIT_FAILURE;
            }

            if (!fbx_helpers::is_skeleton_root(from_node_ptr))
            {
                std::cerr << "Error: node '" << from_node_name << "' is not a Skeleton root node.\n";
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
            return EXIT_FAILURE;
        }

        if (!fbx_helpers::is_skeleton_root(from))
        {
            std::cerr << "Error: node '" << from_node_name << "' is not a Skeleton root node.\n";
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
                return EXIT_FAILURE;
            }
        }

        from_node_ptr = from;
    }

    if (ancestor_is_skeleton_root(target))
    {
        std::cerr << "Error: an ancestor of '" << target->GetName() << "' is already a Skeleton root node.\n";
        return EXIT_FAILURE;
    }

    if (target->GetNodeAttribute() && !target->GetNull() && !target->GetSkeleton())
    {
        std::cerr << "'" << target->GetName() << "' is not a null node.\n";
        return EXIT_FAILURE;
    }

    fbxsdk::FbxSkeleton *skeleton_attr = target->GetSkeleton();
    if (!skeleton_attr)
    {
        skeleton_attr = fbxsdk::FbxSkeleton::Create(sdk_manager.get(), target->GetName());
        fbxsdk::FbxNodeAttribute *old_attr = target->SetNodeAttribute(skeleton_attr);
        if (old_attr)
            old_attr->Destroy();
    }

    skeleton_attr->SetSkeletonType(fbxsdk::FbxSkeleton::eLimbNode);

    convert_child_nulls_to_limbs(target);

    std::cout << "Converted '" << target->GetName() << "' to a Skeleton root node.\n";

    const int file_format = ascii
                                ? fbx_helpers::get_ascii_writer_index(sdk_manager->GetIOPluginRegistry())
                                : fbx_helpers::get_binary_writer_index(sdk_manager->GetIOPluginRegistry());
    if (!export_scene(scene, sdk_manager.get(), output_file, file_format))
    {
        return EXIT_FAILURE;
    }

    std::cout << "Wrote '" << output_file << "'.\n";
    return EXIT_SUCCESS;
}

#pragma endregion

int main(int argc, char **argv)
{
    CLI::App app{"FBX armature re-rooting utility"};

    std::string opt_input_file;
    std::string opt_hierarchy;
    std::string opt_create_hierarchy;
    std::string opt_node_name;
    std::string opt_output_file;
    std::string opt_output_format = "auto";
    bool opt_tree = false;
    bool opt_create_parents = false;

    std::string opt_from_node;
    int opt_to_parent = -1;
    bool opt_yes = false;

    auto *find_command = app.add_subcommand("find", "Find nodes marked as the root Skeleton node");
    find_command->add_option("input", opt_input_file, "Input FBX file")
        ->required()
        ->check(CLI::ExistingFile);
    find_command->add_flag("-t,--tree", opt_tree, "Show the full ancestor path from root to the node itself");

    auto *get_command = app.add_subcommand("get", "Find a node by its full hierarchy path");
    get_command->add_option("input", opt_input_file, "Input FBX file")
        ->required()
        ->check(CLI::ExistingFile);
    get_command->add_option("hierarchy", opt_hierarchy, "Comma-separated hierarchy path from the scene root")
        ->required();

    auto *create_command = app.add_subcommand("create", "Create a Null node at a hierarchy path if it does not exist");
    create_command->add_option("input", opt_input_file, "Input FBX file")
        ->required()
        ->check(CLI::ExistingFile);
    create_command->add_option("hierarchy", opt_create_hierarchy, "Comma-separated hierarchy path from the scene root")
        ->required();
    create_command->add_flag("-p", opt_create_parents, "Create any missing parent nodes in the hierarchy");
    create_command->add_option("-o,--output", opt_output_file, "Output FBX file path (default: <input filename> at CWD)");
    create_command->add_option("-f,--format", opt_output_format, "Output format: auto, ascii, or binary (default: auto)")
        ->default_val("auto")
        ->check(CLI::IsMember({"auto", "ascii", "binary"}));
    create_command->add_flag("-y,--yes", opt_yes, "Auto-confirm overwrite of existing output file");

    app.set_version_flag("--version", PROJECT_VERSION, "Display the program version");

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

    if (app.got_subcommand(find_command))
    {
        return run_find_command(opt_input_file, opt_tree);
    }
    else if (app.got_subcommand(get_command))
    {
        return run_get_command(opt_input_file, opt_hierarchy);
    }
    else if (app.got_subcommand(create_command))
    {
        if (opt_output_file.empty())
            opt_output_file = fs::path(opt_input_file).filename().string();

        return run_create_command(opt_input_file, opt_create_hierarchy, opt_output_file, opt_output_format, opt_yes, opt_create_parents);
    }
    else if (app.got_subcommand(mkroot_command))
    {
        if (opt_output_file.empty())
            opt_output_file = fs::path(opt_input_file).filename().string();

        return run_make_root_command(opt_input_file, opt_node_name, opt_from_node, opt_to_parent, opt_output_file, opt_output_format, opt_yes);
    }

    return EXIT_SUCCESS;
}