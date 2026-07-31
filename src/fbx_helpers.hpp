#pragma once

#include <string>
#include <vector>
#include <fbxsdk.h>

namespace fbx_helpers
{
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