#pragma once

#include <string>
#include <vector>
#include <fbxsdk.h>
#include "fbx_helpers.hpp"

namespace command_helpers
{
    bool confirm_output_overwrite(const std::string &output_file, bool yes);
    bool parse_hierarchy_string(const std::string &hierarchy, std::vector<std::string> &node_names);
    std::string build_hierarchy_string(const std::vector<std::string> &node_names);
    fbxsdk::FbxNode *find_node_by_hierarchy_string(fbxsdk::FbxNode *root,
                                                   const std::string &hierarchy);
    fbxsdk::FbxScene *import_scene(const std::string &input_file,
                                   fbx_helpers::FbxManagerPtr &sdk_manager,
                                   int *input_file_format = nullptr);
    bool export_scene(fbxsdk::FbxScene *scene, const std::string &output_file,
                      const std::string &format, int input_file_format);
}
