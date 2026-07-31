#include "fbx_helpers.hpp"
#include <cstring>

namespace fbx_helpers
{
    bool is_skeleton_root(fbxsdk::FbxNode *node)
    {
        fbxsdk::FbxSkeleton *skeleton = node->GetSkeleton();
        return skeleton && skeleton->IsSkeletonRoot();
    }

    bool is_null_type(fbxsdk::FbxNode *node)
    {
        if (!node)
            return false;

        fbxsdk::FbxNodeAttribute *attr = node->GetNodeAttribute();
        return !attr || node->GetNull() != nullptr;
    }

    int get_ascii_reader_index(const fbxsdk::FbxIOPluginRegistry *registry)
    {
        static int reader_index = -2;
        if (reader_index == -2)
        {
            reader_index = -1;
            int count = registry->GetReaderFormatCount();
            for (int i = 0; i < count; ++i)
            {
                const char *desc = registry->GetReaderFormatDescription(i);
                if (desc && strstr(desc, "ascii"))
                {
                    reader_index = i;
                    break;
                }
            }
        }

        return reader_index;
    }

    int get_ascii_writer_index(const fbxsdk::FbxIOPluginRegistry *registry)
    {
        static int writer_index = -2;
        if (writer_index == -2)
        {
            writer_index = -1;
            int count = registry->GetWriterFormatCount();
            for (int i = 0; i < count; ++i)
            {
                const char *desc = registry->GetWriterFormatDescription(i);
                if (desc && strstr(desc, "ascii"))
                {
                    writer_index = i;
                    break;
                }
            }
        }

        return writer_index;
    }

    int get_binary_writer_index(const fbxsdk::FbxIOPluginRegistry *registry)
    {
        static int writer_index = -2;
        if (writer_index == -2)
        {
            writer_index = -1;
            int count = registry->GetWriterFormatCount();
            for (int i = 0; i < count; ++i)
            {
                const char *desc = registry->GetWriterFormatDescription(i);
                if (desc && strstr(desc, "binary"))
                {
                    writer_index = i;
                    break;
                }
            }
        }

        return writer_index;
    }
} // namespace fbx_helpers