#include "fbx_helpers.hpp"
#include <fbxsdk.h>

namespace fbx_helpers
{
    bool is_skeleton_root(fbxsdk::FbxNode *node)
    {
        fbxsdk::FbxSkeleton *skeleton = node->GetSkeleton();
        return skeleton && skeleton->IsSkeletonRoot();
    }

    void find_skeleton_roots(fbxsdk::FbxNode *node,
                             std::vector<fbxsdk::FbxNode *> &results)
    {
        if (!node)
            return;

        if (is_skeleton_root(node))
        {
            results.push_back(node);
        }

        int child_count = node->GetChildCount();
        for (int i = 0; i < child_count; ++i)
            find_skeleton_roots(node->GetChild(i), results);
    }

    bool is_null_type(fbxsdk::FbxNode *node)
    {
        if (!node)
            return false;

        fbxsdk::FbxNodeAttribute *attr = node->GetNodeAttribute();
        return !attr || node->GetNull() != nullptr;
    }
} // namespace fbx_helpers