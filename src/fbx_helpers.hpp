#pragma once

#include <string>
#include <vector>

namespace fbxsdk
{
    class FbxNode;
}

namespace fbx_helpers
{
    /// Check whether a node is a Skeleton root.
    bool is_skeleton_root(fbxsdk::FbxNode *node);

    /// Recursively collect all Skeleton root nodes under \a node.
    void find_skeleton_roots(fbxsdk::FbxNode *node,
                             std::vector<fbxsdk::FbxNode *> &results);

} // namespace fbx_helpers