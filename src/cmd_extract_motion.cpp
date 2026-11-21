#include "commands.hpp"
#include "command_helpers.hpp"
#include "fbx_helpers.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <fbxsdk.h>

enum class TranslationAxis : unsigned int
{
    None = 0,
    X = 1u << 0,
    Y = 1u << 1,
    Z = 1u << 2
};

static std::string hierarchy;
static TranslationAxis translation_axes = TranslationAxis::None;
static fbxsdk::FbxDouble3 rotation_axis_x(1.0, 0.0, 0.0);
static fbxsdk::FbxDouble3 rotation_axis_y(0.0, 1.0, 0.0);
static fbxsdk::FbxDouble3 rotation_axis_z(0.0, 0.0, 1.0);
static fbxsdk::FbxDouble3 *rotation_axis = nullptr;

static void run_extract_motion_command(const std::string &input_file, const std::string &hierarchy,
                                       TranslationAxis translation_axes, const fbxsdk::FbxDouble3 *rotation_axis,
                                       const std::string &output_file, const std::string &format, bool yes);

static constexpr TranslationAxis operator|(TranslationAxis left, TranslationAxis right)
{
    return static_cast<TranslationAxis>(static_cast<unsigned int>(left) | static_cast<unsigned int>(right));
}

static constexpr TranslationAxis operator&(TranslationAxis left, TranslationAxis right)
{
    return static_cast<TranslationAxis>(static_cast<unsigned int>(left) & static_cast<unsigned int>(right));
}

struct RootMotionSample
{
    fbxsdk::FbxTime time;
    fbxsdk::FbxDouble3 parent_translation;
    fbxsdk::FbxDouble3 node_translation;
    fbxsdk::FbxVector4 parent_rotation;
    fbxsdk::FbxVector4 node_rotation;
};

static TranslationAxis parse_translation_axes(const std::string &axes)
{
    if (!std::all_of(axes.begin(), axes.end(), [](char axis)
                     { return axis == 'x' || axis == 'y' || axis == 'z'; }) ||
        std::adjacent_find(axes.begin(), axes.end()) != axes.end() ||
        std::count(axes.begin(), axes.end(), 'x') > 1 ||
        std::count(axes.begin(), axes.end(), 'y') > 1 ||
        std::count(axes.begin(), axes.end(), 'z') > 1)
        throw CLI::ValidationError("translation", "must be a combination of x, y, and z without duplicates");

    TranslationAxis result = TranslationAxis::None;
    for (char axis : axes)
    {
        if (axis == 'x')
            result = result | TranslationAxis::X;
        else if (axis == 'y')
            result = result | TranslationAxis::Y;
        else if (axis == 'z')
            result = result | TranslationAxis::Z;
    }
    return result;
}

static void register_extract_motion_command(CLI::App &app, CommandContext &context)
{
    auto *command = app.add_subcommand("extract-motion", "Extract a node's root motion to its parent");

    context.add_input_fbx_file_option(*command);
    context.add_output_fbx_file_option(*command);
    context.add_output_fbx_format_option(*command);
    context.add_output_overwrite_flag(*command);

    command->add_option("hierarchy", hierarchy, "Comma-separated hierarchy path from the scene root")
        ->required();
    command->add_option_function<std::string>("-t,--translation", [](const std::string &axes)
                                              { translation_axes = parse_translation_axes(axes); }, "Translation axes to extract (any combination of x, y, and z)")
        ->default_val("");
    command->add_option_function<std::string>("-r,--rotation", [](const std::string &axis)
                                              {
                                                  if (axis == "x")
                                                      rotation_axis = &rotation_axis_x;
                                                  else if (axis == "y")
                                                      rotation_axis = &rotation_axis_y;
                                                  else if (axis == "z")
                                                      rotation_axis = &rotation_axis_z;
                                                  else
                                                      throw CLI::ValidationError("rotation", "must be x, y, or z"); }, "Ground-plane rotation axis to extract (x, y, or z)")
        ->default_val("");

    command->callback([&context]()
                      { run_extract_motion_command(context.input_file, hierarchy,
                                                   translation_axes, rotation_axis,
                                                   context.get_output_fbx_file(), context.output_fbx_format, context.yes); });
}

REGISTER_CLI_SETUP(register_extract_motion_command)

static fbxsdk::FbxQuaternion get_rotation_twist(const fbxsdk::FbxQuaternion &rotation,
                                                const fbxsdk::FbxDouble3 &axis)
{
    const double axis_length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (axis_length < 1e-12)
        return fbxsdk::FbxQuaternion();

    fbxsdk::FbxDouble3 normalized_axis(axis[0] / axis_length, axis[1] / axis_length, axis[2] / axis_length);
    const double projected = rotation[0] * normalized_axis[0] +
                             rotation[1] * normalized_axis[1] +
                             rotation[2] * normalized_axis[2];
    const double length = std::sqrt(projected * projected + rotation[3] * rotation[3]);
    if (length < 1e-12)
        return fbxsdk::FbxQuaternion();

    const double angle = 2.0 * std::atan2(projected / length, rotation[3] / length);
    return fbxsdk::FbxQuaternion(normalized_axis, angle * 180.0 / std::acos(-1.0));
}

static void apply_root_motion(TranslationAxis translation_axes,
                              const fbxsdk::FbxDouble3 *rotation_axis,
                              const fbxsdk::FbxDouble3 &node_translation,
                              const fbxsdk::FbxDouble3 &node_default_translation,
                              const fbxsdk::FbxVector4 &node_rotation,
                              fbxsdk::EFbxRotationOrder node_rotation_order,
                              const fbxsdk::FbxDouble3 &parent_translation,
                              const fbxsdk::FbxVector4 &parent_rotation,
                              fbxsdk::EFbxRotationOrder parent_rotation_order,
                              fbxsdk::FbxDouble3 &node_new_translation,
                              fbxsdk::FbxVector4 &node_new_rotation,
                              fbxsdk::FbxDouble3 &parent_new_translation,
                              fbxsdk::FbxVector4 &parent_new_rotation)
{
    node_new_translation = node_translation;
    node_new_rotation = node_rotation;
    parent_new_translation = parent_translation;
    parent_new_rotation = parent_rotation;

    fbxsdk::FbxDouble3 translation_delta(
        node_translation[0] - node_default_translation[0],
        node_translation[1] - node_default_translation[1],
        node_translation[2] - node_default_translation[2]);

    if ((translation_axes & TranslationAxis::X) != TranslationAxis::None)
    {
        parent_new_translation[0] += translation_delta[0];
        node_new_translation[0] -= translation_delta[0];
    }

    if ((translation_axes & TranslationAxis::Y) != TranslationAxis::None)
    {
        parent_new_translation[1] += translation_delta[1];
        node_new_translation[1] -= translation_delta[1];
    }

    if ((translation_axes & TranslationAxis::Z) != TranslationAxis::None)
    {
        parent_new_translation[2] += translation_delta[2];
        node_new_translation[2] -= translation_delta[2];
    }

    if (rotation_axis)
    {
        fbxsdk::FbxQuaternion parent_quaternion = fbx_helpers::rotation_to_quaternion(parent_rotation, parent_rotation_order);
        fbxsdk::FbxQuaternion node_quaternion = fbx_helpers::rotation_to_quaternion(node_rotation, node_rotation_order);
        fbxsdk::FbxQuaternion twist = get_rotation_twist(node_quaternion, *rotation_axis);
        fbxsdk::FbxQuaternion inverse_twist = twist;
        inverse_twist.Inverse();
        node_new_rotation = fbx_helpers::quaternion_to_rotation(inverse_twist * node_quaternion, node_rotation_order);
        parent_new_rotation = fbx_helpers::quaternion_to_rotation(parent_quaternion * twist, parent_rotation_order);
    }
}

static bool update_default_transform(fbxsdk::FbxNode *node,
                                     TranslationAxis translation_axes,
                                     const fbxsdk::FbxDouble3 *rotation_axis,
                                     const fbxsdk::FbxDouble3 &node_default_translation)
{
    fbxsdk::FbxNode *parent = node->GetParent();
    fbxsdk::EFbxRotationOrder parent_rotation_order;
    fbxsdk::EFbxRotationOrder node_rotation_order;
    parent->GetRotationOrder(fbxsdk::FbxNode::eSourcePivot, parent_rotation_order);
    node->GetRotationOrder(fbxsdk::FbxNode::eSourcePivot, node_rotation_order);

    fbxsdk::FbxDouble3 node_new_translation;
    fbxsdk::FbxVector4 node_new_rotation;
    fbxsdk::FbxDouble3 parent_new_translation;
    fbxsdk::FbxVector4 parent_new_rotation;

    apply_root_motion(
        translation_axes,
        rotation_axis,
        node->LclTranslation.Get(),
        node_default_translation,
        node->LclRotation.Get(),
        node_rotation_order,
        parent->LclTranslation.Get(),
        parent->LclRotation.Get(),
        parent_rotation_order,
        node_new_translation,
        node_new_rotation,
        parent_new_translation,
        parent_new_rotation);

    parent->LclTranslation.Set(parent_new_translation);
    parent->LclRotation.Set(parent_new_rotation);
    node->LclTranslation.Set(node_new_translation);
    node->LclRotation.Set(node_new_rotation);

    return true;
}

static void get_animation_stack_range(fbxsdk::FbxAnimStack *stack, fbxsdk::FbxNode *node,
                                      fbxsdk::FbxTime *start, fbxsdk::FbxTime *stop)
{
    *start = stack->LocalStart.Get();
    *stop = stack->LocalStop.Get();

    const int num_layers = stack->GetMemberCount<fbxsdk::FbxAnimLayer>();
    const std::array<fbxsdk::FbxProperty *, 2> properties = {&node->LclTranslation, &node->LclRotation};

    for (int i = 0; i < num_layers; ++i)
    {
        fbxsdk::FbxAnimLayer *layer = stack->GetMember<fbxsdk::FbxAnimLayer>(i);
        for (fbxsdk::FbxProperty *property : properties)
        {
            for (const char *channel : {"X", "Y", "Z"})
            {
                fbxsdk::FbxAnimCurve *curve = property->GetCurve(layer, channel, false);
                if (!curve || curve->KeyGetCount() == 0)
                    continue;

                fbxsdk::FbxTime first_key_time = curve->KeyGetTime(0);
                fbxsdk::FbxTime last_key_time = curve->KeyGetTime(curve->KeyGetCount() - 1);
                if (first_key_time < *start)
                    *start = first_key_time;
                if (last_key_time > *stop)
                    *stop = last_key_time;
            }
        }
    }
}

static void get_animation_stack_range_with_parent(
    fbxsdk::FbxAnimStack *stack,
    fbxsdk::FbxNode *node,
    fbxsdk::FbxTime *start,
    fbxsdk::FbxTime *stop)
{
    fbxsdk::FbxTime parent_start;
    fbxsdk::FbxTime parent_stop;
    get_animation_stack_range(stack, node, start, stop);
    get_animation_stack_range(stack, node->GetParent(), &parent_start, &parent_stop);

    if (parent_start < *start)
        *start = parent_start;
    if (parent_stop > *stop)
        *stop = parent_stop;
}

static void write_curve_samples(fbxsdk::FbxAnimLayer *layer,
                                fbxsdk::FbxProperty &property,
                                const char *channel, const std::vector<RootMotionSample> &samples,
                                bool parent_property, bool rotation_property, int component)
{
    fbxsdk::FbxAnimCurve *curve = property.GetCurve(layer, channel, true);

    curve->KeyModifyBegin();
    curve->KeyClear();

    for (const RootMotionSample &sample : samples)
    {
        const double value = rotation_property
                                 ? (parent_property ? sample.parent_rotation[component] : sample.node_rotation[component])
                                 : (parent_property ? sample.parent_translation[component] : sample.node_translation[component]);
        int key_index = curve->KeyAdd(sample.time);
        curve->KeySet(key_index, sample.time, static_cast<float>(value),
                      fbxsdk::FbxAnimCurveDef::eInterpolationLinear);
    }

    curve->KeyModifyEnd();
}

static bool has_root_motion_animation(fbxsdk::FbxAnimStack *stack,
                                      fbxsdk::FbxNode *node,
                                      TranslationAxis translation_axes,
                                      const fbxsdk::FbxDouble3 *rotation_axis)
{
    for (int i = 0; i < stack->GetMemberCount<fbxsdk::FbxAnimLayer>(); ++i)
    {
        fbxsdk::FbxAnimLayer *layer = stack->GetMember<fbxsdk::FbxAnimLayer>(i);

        if ((translation_axes & TranslationAxis::X) != TranslationAxis::None)
        {
            fbxsdk::FbxAnimCurve *curve = node->LclTranslation.GetCurve(layer, "X", false);
            if (curve && curve->KeyGetCount() > 0)
                return true;
        }

        if ((translation_axes & TranslationAxis::Y) != TranslationAxis::None)
        {
            fbxsdk::FbxAnimCurve *curve = node->LclTranslation.GetCurve(layer, "Y", false);
            if (curve && curve->KeyGetCount() > 0)
                return true;
        }

        if ((translation_axes & TranslationAxis::Z) != TranslationAxis::None)
        {
            fbxsdk::FbxAnimCurve *curve = node->LclTranslation.GetCurve(layer, "Z", false);
            if (curve && curve->KeyGetCount() > 0)
                return true;
        }

        if (rotation_axis)
        {
            for (const char *channel : {"X", "Y", "Z"})
            {
                fbxsdk::FbxAnimCurve *curve = node->LclRotation.GetCurve(layer, channel, false);
                if (curve && curve->KeyGetCount() > 0)
                    return true;
            }
        }
    }

    return false;
}

static bool extract_root_motion(fbxsdk::FbxAnimStack *stack,
                                fbxsdk::FbxNode *node,
                                const fbxsdk::FbxDouble3 &node_default_translation,
                                TranslationAxis translation_axes,
                                const fbxsdk::FbxDouble3 *rotation_axis)
{
    fbxsdk::FbxScene *scene = stack->GetScene();
    if (!scene)
    {
        std::cerr << "Error: animation stack '" << stack->GetName() << "' is not attached to a scene.\n";
        return false;
    }

    fbxsdk::FbxNode *parent = node->GetParent();
    if (!parent || parent == scene->GetRootNode())
    {
        std::cerr << "Node has no parent.\n";
        return false;
    }

    fbxsdk::FbxAnimLayer *base_layer = stack->GetMember<fbxsdk::FbxAnimLayer>(0);
    if (!base_layer)
    {
        std::cerr << "Error: animation stack '" << stack->GetName() << "' has no animation layer.\n";
        return false;
    }

    fbxsdk::FbxTime::EMode time_mode = scene->GetGlobalSettings().GetTimeMode();
    if (fbxsdk::FbxTime::GetFrameRate(time_mode) <= 0.0)
    {
        std::cerr << "Error: invalid frame rate in the scene settings.\n";
        return false;
    }

    fbxsdk::FbxTime start;
    fbxsdk::FbxTime stop;

    get_animation_stack_range_with_parent(stack, node, &start, &stop);

    if (stop < start)
    {
        std::cerr << "Error: invalid time range in animation stack '" << stack->GetName() << "'.\n";
        return false;
    }

    scene->SetCurrentAnimationStack(stack);

    fbxsdk::EFbxRotationOrder parent_rotation_order;
    fbxsdk::EFbxRotationOrder node_rotation_order;
    parent->GetRotationOrder(fbxsdk::FbxNode::eSourcePivot, parent_rotation_order);
    node->GetRotationOrder(fbxsdk::FbxNode::eSourcePivot, node_rotation_order);

    // Obtain the timestamps for each sample

    const fbxsdk::FbxLongLong first_frame = static_cast<fbxsdk::FbxLongLong>(std::ceil(start.GetFrameCountPrecise(time_mode)));
    const fbxsdk::FbxLongLong last_frame = static_cast<fbxsdk::FbxLongLong>(std::floor(stop.GetFrameCountPrecise(time_mode)));

    std::vector<fbxsdk::FbxTime> sample_times;
    sample_times.push_back(start);

    for (fbxsdk::FbxLongLong frame = first_frame; frame <= last_frame; ++frame)
    {
        fbxsdk::FbxTime time;
        time.SetFrame(frame, time_mode);
        if (time > start && time < stop)
            sample_times.push_back(time);
    }

    if (stop > start)
        sample_times.push_back(stop);

    // Make samples.

    std::vector<RootMotionSample> samples;
    samples.reserve(sample_times.size());

    fbxsdk::FbxDouble3 node_new_translation;
    fbxsdk::FbxVector4 node_new_rotation;
    fbxsdk::FbxDouble3 parent_new_translation;
    fbxsdk::FbxVector4 parent_new_rotation;

    for (const fbxsdk::FbxTime &time : sample_times)
    {
        RootMotionSample sample;
        sample.time = time;

        apply_root_motion(
            translation_axes,
            rotation_axis,
            node->EvaluateLocalTranslation(time, fbxsdk::FbxNode::eSourcePivot, false, true),
            node_default_translation,
            node->EvaluateLocalRotation(time, fbxsdk::FbxNode::eSourcePivot, false, true),
            node_rotation_order,
            parent->EvaluateLocalTranslation(time, fbxsdk::FbxNode::eSourcePivot, false, true),
            parent->EvaluateLocalRotation(time, fbxsdk::FbxNode::eSourcePivot, false, true),
            parent_rotation_order,
            sample.node_translation,
            sample.node_rotation,
            sample.parent_translation,
            sample.parent_rotation);

        samples.push_back(sample);
    }

    for (int component = 0; component < 3; ++component)
    {
        const char *channel = component == 0 ? "X" : component == 1 ? "Y"
                                                                    : "Z";
        if ((translation_axes & static_cast<TranslationAxis>(1u << component)) != TranslationAxis::None)
        {
            write_curve_samples(base_layer, parent->LclTranslation, channel, samples, true, false, component);
            write_curve_samples(base_layer, node->LclTranslation, channel, samples, false, false, component);
        }
        if (rotation_axis)
        {
            write_curve_samples(base_layer, parent->LclRotation, channel, samples, true, true, component);
            write_curve_samples(base_layer, node->LclRotation, channel, samples, false, true, component);
        }
    }

    return true;
}

static void run_extract_motion_command(const std::string &input_file, const std::string &hierarchy,
                                       TranslationAxis translation_axes, const fbxsdk::FbxDouble3 *rotation_axis,
                                       const std::string &output_file, const std::string &format, bool yes)
{
    if (!command_helpers::confirm_output_overwrite(output_file, yes))
        throw CLI::RuntimeError(EXIT_FAILURE);

    if (translation_axes == TranslationAxis::None && !rotation_axis)
    {
        std::cerr << "Error: specify translation axes and/or a rotation axis to extract.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    int input_file_format = -1;
    fbx_helpers::FbxManagerPtr sdk_manager;
    fbxsdk::FbxScene *scene = command_helpers::import_scene(input_file, sdk_manager, &input_file_format);
    if (!scene)
        throw CLI::RuntimeError(EXIT_FAILURE);

    fbxsdk::FbxNode *node = command_helpers::find_node_by_hierarchy_string(scene->GetRootNode(), hierarchy);
    if (!node)
    {
        std::cerr << "Error: hierarchy '" << hierarchy << "' not found.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    if (!node->GetParent() || node->GetParent() == scene->GetRootNode())
    {
        std::cerr << "Error: node '" << hierarchy << "' must have a parent other than the scene root.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    // Go through all the animation stacks and bake their layers into one.
    // Since this is about extracting root motion it doesn't make much
    // sense to go through each layer individually. The only thing that matters
    // is the evaluated local transforms between the node and the parent.

    const int stack_count = scene->GetSrcObjectCount<fbxsdk::FbxAnimStack>();
    std::vector<fbxsdk::FbxAnimStack *> stacks_with_root_motion;

    for (int i = 0; i < stack_count; ++i)
    {
        fbxsdk::FbxAnimStack *stack = scene->GetSrcObject<fbxsdk::FbxAnimStack>(i);

        if (!has_root_motion_animation(stack, node, translation_axes, rotation_axis))
            continue;

        stacks_with_root_motion.push_back(stack);

        scene->SetCurrentAnimationStack(stack);

        if (stack->GetMemberCount<fbxsdk::FbxAnimLayer>() > 1)
        {
            fbxsdk::FbxTime start;
            fbxsdk::FbxTime stop;
            get_animation_stack_range_with_parent(stack, node, &start, &stop);

            fbxsdk::FbxTime period;
            period.SetSecondDouble(1.0 / fbxsdk::FbxTime::GetFrameRate(scene->GetGlobalSettings().GetTimeMode()));

            if (!stack->BakeLayers(scene->GetAnimationEvaluator(), start, stop, period))
            {
                std::cerr << "Error: could not bake animation layers in stack '" << stack->GetName() << "'.\n";
                throw CLI::RuntimeError(EXIT_FAILURE);
            }
        }
    }

    if (stacks_with_root_motion.empty())
    {
        std::cerr << "Error: no animation stack contains keyed channels selected for root-motion extraction.\n";
        throw CLI::RuntimeError(EXIT_FAILURE);
    }

    // Update the node's default transform.
    // NOTE: Does the binding/rest poses need to be adjusted as well?

    const fbxsdk::FbxDouble3 node_default_translation(node->LclTranslation);
    update_default_transform(node, translation_axes, rotation_axis, node_default_translation);

    // Extract root motion.

    for (fbxsdk::FbxAnimStack *stack : stacks_with_root_motion)
    {
        if (!extract_root_motion(stack, node, node_default_translation, translation_axes, rotation_axis))
            throw CLI::RuntimeError(EXIT_FAILURE);
    }

    if (!command_helpers::export_scene(scene, output_file, format, input_file_format))
        throw CLI::RuntimeError(EXIT_FAILURE);

    std::cout << "Extracted root motion from '" << node->GetName() << "' to its parent.\n"
              << "Wrote '" << output_file << "'.\n";
}
