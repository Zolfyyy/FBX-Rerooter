#pragma once

#include <memory>
#include <string>
#include <vector>
#include <fbxsdk.h>

namespace fbx_helpers
{
    struct FbxManagerDeleter
    {
        void operator()(fbxsdk::FbxManager *manager) const
        {
            if (manager)
                manager->Destroy();
        }
    };

    using FbxManagerPtr = std::unique_ptr<fbxsdk::FbxManager, FbxManagerDeleter>;

    /// Convert Euler rotation angles to a normalized quaternion using the specified rotation order.
    fbxsdk::FbxQuaternion rotation_to_quaternion(const fbxsdk::FbxVector4 &rotation,
                                                 fbxsdk::EFbxRotationOrder order);

    /// Convert a quaternion to Euler rotation angles using the specified rotation order.
    fbxsdk::FbxVector4 quaternion_to_rotation(const fbxsdk::FbxQuaternion &quaternion,
                                              fbxsdk::EFbxRotationOrder order);

    /// Check whether a node is a Skeleton root.
    bool is_skeleton_root(fbxsdk::FbxNode *node);

    /// Check whether a node is a Null type (no attribute or attribute is Null).
    bool is_null_type(fbxsdk::FbxNode *node);

    /// Get the reader format index for ASCII FBX, or -1 if not found.
    int get_ascii_reader_index(const fbxsdk::FbxIOPluginRegistry *registry);

    /// Get the writer format index for ASCII FBX, or -1 if not found.
    int get_ascii_writer_index(const fbxsdk::FbxIOPluginRegistry *registry);

    /// Get the writer format index for binary FBX, or -1 if not found.
    int get_binary_writer_index(const fbxsdk::FbxIOPluginRegistry *registry);

} // namespace fbx_helpers