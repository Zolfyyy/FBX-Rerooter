#include "command_helpers.hpp"
#include "fbx_helpers.hpp"
#include <filesystem>
#include <iostream>

namespace command_helpers
{
    namespace fs = std::filesystem;

    bool confirm_output_overwrite(const std::string &output_file, bool yes)
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

    bool parse_hierarchy_string(const std::string &hierarchy, std::vector<std::string> &node_names)
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

    std::string build_hierarchy_string(const std::vector<std::string> &node_names)
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

    fbxsdk::FbxNode *find_node_by_hierarchy_string(fbxsdk::FbxNode *root,
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

    fbxsdk::FbxScene *import_scene(const std::string &input_file,
                                   fbx_helpers::FbxManagerPtr &sdk_manager,
                                   int *input_file_format)
    {
        sdk_manager.reset(fbxsdk::FbxManager::Create());
        if (!sdk_manager)
        {
            std::cerr << "Fatal: failed to create FBX SDK manager.\n";
            return nullptr;
        }

        fbxsdk::FbxScene *scene = fbxsdk::FbxScene::Create(sdk_manager.get(), "");
        if (!scene)
        {
            std::cerr << "Fatal: failed to create FBX scene.\n";
            return nullptr;
        }

        fbxsdk::FbxManager *scene_manager = scene->GetFbxManager();
        scene_manager->SetIOSettings(fbxsdk::FbxIOSettings::Create(scene_manager, IOSROOT));

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

    bool export_scene(fbxsdk::FbxScene *scene, const std::string &output_file,
                      const std::string &format, int input_file_format)
    {
        if (!scene)
            return false;

        fbxsdk::FbxManager *sdk_manager = scene->GetFbxManager();

        int file_format;
        if (format == "ascii")
            file_format = fbx_helpers::get_ascii_writer_index(sdk_manager->GetIOPluginRegistry());
        else if (format == "binary")
            file_format = fbx_helpers::get_binary_writer_index(sdk_manager->GetIOPluginRegistry());
        else if (format == "auto")
        {
            if (fbx_helpers::get_ascii_reader_index(sdk_manager->GetIOPluginRegistry()) == input_file_format)
                file_format = fbx_helpers::get_ascii_writer_index(sdk_manager->GetIOPluginRegistry());
            else
                file_format = fbx_helpers::get_binary_writer_index(sdk_manager->GetIOPluginRegistry());
        }

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

} // namespace command_helpers
