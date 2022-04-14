/**
* Copyright (c) 2021-2022 Hailo Technologies Ltd. All rights reserved.
* Distributed under the LGPL license (https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt)
**/
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <string>
#include <tuple>
#include <vector>

#include "common/math.hpp"
#include "common/tensors.hpp"
#include "common/nms.hpp"
#include "face_detection.hpp"
#include "xtensor/xadapt.hpp"
#include "xtensor/xarray.hpp"
#include "xtensor/xbuilder.hpp"
#include "xtensor/xcontainer.hpp"
#include "xtensor/xeval.hpp"
#include "xtensor/xtensor.hpp"
#include "xtensor/xindex_view.hpp"
#include "xtensor/xio.hpp"
#include "xtensor/xmanipulation.hpp"
#include "xtensor/xmath.hpp"
#include "xtensor/xoperation.hpp"
#include "xtensor/xpad.hpp"
#include "xtensor/xrandom.hpp"
#include "xtensor/xshape.hpp"
#include "xtensor/xsort.hpp"
#include "xtensor/xstrided_view.hpp"
#include "xtensor/xview.hpp"
#include "hailo_xtensor.hpp"

// Teporary values
#define LIGHTFACE_WIDTH (320)
#define LIGHTFACE_HEIGHT (240)
#define RETINAFACE_WIDTH (1280)
#define RETINAFACE_HEIGHT (736)

//******************************************************************
// SETUP - ANCHOR EXTRACTION
//******************************************************************
xt::xarray<float> get_anchors(const std::vector<std::vector<int>> &anchor_min_sizes,
                              const xt::xarray<int> &anchor_steps,
                              const int width,
                              const int height)
{
    // Here we need to calculate the anchors of the image so we can extract faces later.
    // We start by calculating the feature map sizes based on the anchor steps.
    xt::xarray<int> feature_maps_height = xt::ceil(height / xt::cast<float>(anchor_steps));
    xt::xarray<int> feature_maps_width = xt::ceil(width / xt::cast<float>(anchor_steps));
    xt::xarray<int> feature_maps = xt::stack(xt::xtuple(feature_maps_height, feature_maps_width), 1);

    // Use the feature map to pre-emptively calculate the size of the anchors.
    int num_anchors = 0;
    xt::xarray<int> feature_area = feature_maps_height * feature_maps_width;
    for (uint index = 0; index < anchor_min_sizes.size(); index++)
    {
        num_anchors += feature_area(index) * anchor_min_sizes[index].size();
    }
    // Now initialize the anchors to the given size. This way we can fill them in-place
    // instead of concatenating, saving lots of time.
    xt::xarray<float> anchors = xt::zeros<float>({num_anchors, 4});

    // Calculate the anchors.
    int counter = 0;
    for (uint index = 0; index < feature_maps.shape(0); index++)
    {
        auto current_min_sizes = anchor_min_sizes[index];
        for (int i = 0; i < feature_maps(index, 0); i++)
        {
            for (int j = 0; j < feature_maps(index, 1); j++)
            {
                for (const float &min_size : current_min_sizes)
                {
                    anchors(counter, 0) = CLAMP((j + 0.5) / feature_maps(index, 1), 0.0, 1.0);
                    anchors(counter, 1) = CLAMP((i + 0.5) / feature_maps(index, 0), 0.0, 1.0);
                    anchors(counter, 2) = CLAMP(min_size / width, 0.0, 1.0);
                    anchors(counter, 3) = CLAMP(min_size / height, 0.0, 1.0);
                    counter++;
                }
            }
        }
    }
    return anchors;
}

//******************************************************************
// BOX/LANDMARK DECODING
//******************************************************************
xt::xarray<float> decode_landmarks(const xt::xarray<float> &landmark_detections,
                                   const xt::xarray<float> &anchors,
                                   const xt::xarray<float> &anchors_multiplier)
{
    // Decode the boxes relative to their anchors.
    // There are 5 landmarks paired in sets of 2 (x and y values),
    // so we need to tile our anchors by 5
    xt::xarray<float> landmarks = xt::tile(xt::view(anchors, xt::all(), xt::range(0, 2)), {1, 5}) + landmark_detections * xt::tile(anchors_multiplier, {1, 5});
    return landmarks;
}

xt::xarray<float> decode_boxes(const xt::xarray<float> &box_detections,
                               const xt::xarray<float> &anchors,
                               const xt::xarray<float> &anchors_multiplier,
                               const xt::xarray<float> &anchor_variance)
{
    // Decode the boxes relative to their anchors
    xt::xarray<float> anchored_boxes_1 = xt::view(anchors, xt::all(), xt::range(0, 2)) + xt::view(box_detections, xt::all(), xt::range(0, 2)) * anchors_multiplier;
    xt::xarray<float> anchored_boxes_2 = xt::view(anchors, xt::all(), xt::range(2, 4)) * xt::exp(xt::view(box_detections, xt::all(), xt::range(2, 4)) * anchor_variance(1));
    xt::xarray<float> boxes = xt::concatenate(xt::xtuple(anchored_boxes_1, anchored_boxes_2), 1);

    // We use view assignment to perform operations on just those slices
    auto front_slice = xt::view(boxes, xt::all(), xt::range(0, 2));
    front_slice -= xt::view(boxes, xt::all(), xt::range(2, 4)) / 2;
    auto back_slice = xt::view(boxes, xt::all(), xt::range(2, 4));
    back_slice += xt::view(boxes, xt::all(), xt::range(0, 2));
    return boxes;
}

std::tuple<xt::xarray<float>, xt::xarray<float>, xt::xarray<float>> detect_boxes_and_landmarks(const xt::xarray<float> &box_outputs,
                                                                                               const xt::xarray<float> &class_scores,
                                                                                               const xt::xarray<float> &landmark_ouputs,
                                                                                               const xt::xarray<float> &anchors,
                                                                                               const xt::xarray<float> &anchors_multiplier,
                                                                                               const xt::xarray<float> &anchor_variance,
                                                                                               const float score_threshold)
{
    xt::xarray<float> landmarks;
    // Decode the boxes and get the face scores (we don't care about unlabeled scores)
    xt::xarray<float> boxes = decode_boxes(xt::squeeze(box_outputs), anchors, anchors_multiplier, anchor_variance);
    xt::xarray<float> scores = xt::col(xt::squeeze(class_scores), 1);

    // Filter out low scores
    // Note: xt::where returns a vector of vectors (one for each dim) of indices where condition is true
    auto higher_scores = xt::where(scores > score_threshold);
    boxes = xt::view(boxes, xt::keep(higher_scores[0]), xt::all());
    scores = xt::view(scores, xt::keep(higher_scores[0]));

    // If landmarks are available, then decode those too.
    if (landmark_ouputs.dimension() > 0)
    {
        auto cropped_landmarks = xt::view(xt::squeeze(landmark_ouputs), xt::keep(higher_scores[0]), xt::all());
        auto cropped_anchors = xt::view(anchors, xt::keep(higher_scores[0]), xt::all());
        auto cropped_anchors_mul = xt::view(anchors_multiplier, xt::keep(higher_scores[0]), xt::all());
        landmarks = decode_landmarks(cropped_landmarks, cropped_anchors, cropped_anchors_mul);
    }

    return std::tuple<xt::xarray<float>, xt::xarray<float>, xt::xarray<float>>(std::move(boxes), std::move(scores), std::move(landmarks));
}

//******************************************************************
// DETECTION/LANDMARKS EXTRACTION & ENCODING
//******************************************************************
void encode_detections(std::vector<NewHailoDetection> &objects,
                       xt::xarray<float> &detection_boxes,
                       xt::xarray<float> &scores,
                       xt::xarray<float> &landmarks)
{
    // Here we will package the processed detections into the NewHailoDetection meta
    // The detection meta will hold the following items:
    float confidence, w, h, xmin, ymin = 0.0f;
    // There is only 1 class in this network (face) so there is no need for label.
    std::string label = "face";
    // Iterate over our results
    for (uint index = 0; index < scores.size(); ++index)
    {
        confidence = scores(index);                                  // Get the score for this detection
        xmin = detection_boxes(index, 0);                            // Box xmin, relative to image size
        ymin = detection_boxes(index, 1);                            // Box ymin, relative to image size
        w = (detection_boxes(index, 2) - detection_boxes(index, 0)); // Box width, relative to image size
        h = (detection_boxes(index, 3) - detection_boxes(index, 1)); // Box height, relative to image size

        // Once all parameters are calculated, push them into the meta
        // Class = 1 since centerpose only detects people
        HailoBBox bbox(xmin, ymin, w, h);
        NewHailoDetection detected_face(bbox, label, confidence);

        if (landmarks.dimension() > 0)
        {
            xt::xarray<float> keypoints_raw = xt::row(landmarks, index);
            // The keypoints are flatten, reshape them to 2 * num_keypoints.
            int num_keypoints = keypoints_raw.shape(0) / 2;
            auto face_keypoints = xt::reshape_view(keypoints_raw, {num_keypoints, 2});
            hailo_common::add_landmarks_to_detection(detected_face, "retinaface", face_keypoints);
        }

        objects.push_back(detected_face); // Push the detection to the objects vector
    }
}

std::vector<NewHailoDetection> face_detection_postprocess(std::vector<NewHailoTensorPtr> &tensors,
                                                          const xt::xarray<float> &anchors,
                                                          const xt::xarray<float> &anchors_multiplier,
                                                          const xt::xarray<float> &anchor_variance,
                                                          const float score_threshold,
                                                          const float iou_threshold,
                                                          const int num_branches)
{
    std::vector<NewHailoDetection> objects; // The detection meta we will eventually return

    //-------------------------------
    // TENSOR GATHERING
    //-------------------------------

    int num_outputs = tensors.size();
    int outputs_per_branch = num_outputs / num_branches;
    // The output layers fall into hree categories: boxes, classes(scores), and lanmarks(x,y for each)
    std::vector<xt::xarray<float>> box_layers;
    std::vector<xt::xarray<float>> class_layers;
    std::vector<xt::xarray<float>> landmarks_layers;

    std::size_t boxes_reshaped_size = 0;
    std::size_t landmarks_reshaped_size = 0;
    std::size_t classes_reshaped_size = 0;

    // Separate the layers by outs_per_branch steps
    for (uint i = 0; i < tensors.size(); ++i)
    {
        // While we're here, adapt the tensor into an xarray of float (dequantized).
        xt::xarray<uint8_t> xdata = common::get_xtensor(tensors[i]);
        xt::xarray<float> xdata_rescaled = common::dequantize(xdata, tensors[i]->vstream_info().quant_info.qp_scale, tensors[i]->vstream_info().quant_info.qp_zp);
        // output layers are paired: boxes:classes:landmarks, boxes:classes:landmarks, boxes:classes:landmarks, etc...
        if (i % outputs_per_branch == 0)
        {
            auto num_boxes = (int)xdata_rescaled.shape(0) * (int)xdata_rescaled.shape(1) * ((int)xdata_rescaled.shape(2) / 4);
            auto xdata_reshaped = xt::reshape_view(xdata_rescaled, {1, num_boxes, 4}); // Resize to be by the 4 parameters for a box
            box_layers.emplace_back(std::move(xdata_reshaped));
            boxes_reshaped_size += num_boxes;
        }
        else if (i % outputs_per_branch == 1)
        {
            auto num_classes = (int)xdata_rescaled.shape(0) * (int)xdata_rescaled.shape(1) * ((int)xdata_rescaled.shape(2) / 2);
            auto xdata_reshaped = xt::reshape_view(xdata_rescaled, {1, num_classes, 2}); // Resize to be by the 2 available classes
            class_layers.emplace_back(std::move(xdata_reshaped));
            classes_reshaped_size += num_classes;
        }
        else
        {
            auto num_landmarks = (int)xdata_rescaled.shape(0) * (int)xdata_rescaled.shape(1) * ((int)xdata_rescaled.shape(2) / 10);
            auto xdata_reshaped = xt::reshape_view(xdata_rescaled, {1, num_landmarks, 10}); // Resize to be by the (x,y) for each of the 5 landmarks (2*5=10)
            landmarks_layers.emplace_back(std::move(xdata_reshaped));
            landmarks_reshaped_size += num_landmarks;
        }
    }

    // Sort the two sets in descending order so their order lines up with the pre-calculated anchors.
    std::sort(box_layers.begin(), box_layers.end(), [](const auto &lhs, const auto &rhs)
              { return rhs.shape(1) < lhs.shape(1); });
    std::sort(class_layers.begin(), class_layers.end(), [](const auto &lhs, const auto &rhs)
              { return rhs.shape(1) < lhs.shape(1); });

    // Concatenate the different output layers together. second dim (1, N, 4) or (1, N, 2) should
    // now match the anchors_multipler devised earlier.
    int index = 0;
    std::vector<std::size_t> boxes_shape = {1, boxes_reshaped_size, 4};
    xt::xarray<float> stacked_boxes(boxes_shape);
    for (uint i = 0; i < box_layers.size(); ++i)
    {
        xt::view(stacked_boxes, xt::all(), xt::range(index, index + box_layers[i].shape(1)), xt::all()) = box_layers[i];
        index += box_layers[i].shape(1);
    }

    std::vector<std::size_t> classes_shape = {1, classes_reshaped_size, 2};
    // We tell xtensor to explicitly layout the memory by rows, in order to perform math ops faster, e.g. softmax.
    xt::xarray<float, xt::layout_type::row_major> stacked_classes(classes_shape);
    index = 0;
    for (uint i = 0; i < class_layers.size(); ++i)
    {
        xt::view(stacked_classes, xt::all(), xt::range(index, index + class_layers[i].shape(1)), xt::all()) = class_layers[i];
        index += class_layers[i].shape(1);
    }

    // If there are landmarks, concat those too.
    std::vector<std::size_t> landmarks_shape = {};
    if (landmarks_reshaped_size > 0)
    {
        landmarks_shape = {1, landmarks_reshaped_size, 10};
    }
    xt::xarray<float> stacked_landmarks(landmarks_shape);
    index = 0;

    if (landmarks_layers.size() > 0)
    {
        for (uint i = 0; i < landmarks_layers.size(); ++i)
        {
            xt::view(stacked_landmarks, xt::all(), xt::range(index, index + landmarks_layers[i].shape(1)), xt::all()) = landmarks_layers[i];
            index += landmarks_layers[i].shape(1);
        }
    }
    //-------------------------------
    // CALCULATION AND EXTRACTION
    //-------------------------------

    // Run softmax on the scores to get the relevant class
    // xt::xarray<float> class_scores = common::softmax_xtensor(stacked_classes);
    common::softmax_2D(stacked_classes.data(), stacked_classes.shape(1), stacked_classes.shape(2));

    // Extract boxes and landmarks
    auto boxes_and_landmarks = detect_boxes_and_landmarks(stacked_boxes, stacked_classes, stacked_landmarks,
                                                          anchors, anchors_multiplier, anchor_variance,
                                                          score_threshold);

    //-------------------------------
    // RESULTS ENCODING
    //-------------------------------

    // Encode the individual boxes/keypoints and package them into the meta
    encode_detections(objects,
                      std::get<0>(boxes_and_landmarks),
                      std::get<1>(boxes_and_landmarks),
                      std::get<2>(boxes_and_landmarks));

    // Perform nms to throw out similar detections
    common::nms(objects, iou_threshold);

    return objects;
}

//******************************************************************
//  RETINAFACE POSTPROCESS
//******************************************************************
void retinaface(HailoROIPtr roi)
{
    /*
     *  The retinaface operates under the same principles as the lightface network
     *  below, however here we include a set of landmarks for each detection box.
     *  This means that instead of sets of 2 tensors, the newtork outputs 3 sets
     *  of 3 tensors (totalling in 9 output layers). So each set has a tensor for
     *  boxes, corresponding scores, and corresponding landmarks. Like in lightface,
     * we need to trabsform the boxes using anchors determined by the parameters below.
     */
    // Get the output layers from the hailo frame.
    if (!roi->has_tensors())
    {
        return;
    }
    std::vector<NewHailoTensorPtr> tensors = roi->get_tensors();
    // Network specific parameters
    const int image_width = RETINAFACE_WIDTH;
    const int image_height = RETINAFACE_HEIGHT;
    const xt::xarray<float> anchor_variance = {0.1, 0.2};
    const xt::xarray<int> anchor_steps = {8, 16, 32};
    const std::vector<std::vector<int>> anchor_min_size = {{16, 32}, {64, 128}, {256, 512}};
    const int num_branches = anchor_min_size.size();
    const float score_threshold = 0.4;
    const float iou_threshold = 0.4;

    // Calculate the anchors based on the image size, step size, and feature map.
    const xt::xarray<float> anchors = get_anchors(anchor_min_size, anchor_steps, image_width, image_height);
    // Using the anchors, create a multiplier that will be used against the tensor results.
    const xt::xarray<float> anchors_multiplier = anchor_variance(0) * xt::view(anchors, xt::all(), xt::range(2, _));

    // No need to Rearrange the order of the output layers to match boxes:classes:landmarks
    std::rotate(tensors.begin(), tensors.begin() + 6, tensors.end());
    std::rotate(tensors.begin() + 3, tensors.begin() + 6, tensors.end());

    // Extract the detection objects using the given parameters.
    std::vector<NewHailoDetection> detections = face_detection_postprocess(tensors, anchors, anchors_multiplier, anchor_variance,
                                                                           score_threshold, iou_threshold, num_branches);

    // Update the frame with the found detections.
    hailo_common::add_detections(roi, detections);
}

//******************************************************************
//  LIGHTFACE POSTPROCESS
//******************************************************************
std::vector<NewHailoDetection> lightface_post(std::vector<NewHailoTensorPtr> &tensors)
{
    /*
     *  The lightface network outputs tensors in 4 sets of 2 (totaling 8 layers).
     *  Each set has a layer describing bounding boxes of detections, and a layer
     *  of class scores for each corresponding box. Each set of boxes and scores
     *  operate at a different scale of the feature set. The 4 different scales
     *  give us decent coverage of the possible sizes objects can take in the image.
     *  Since the detections output by the network are anchor boxes, we need to
     *  multiply them by anchors that we determine in advanced using the parameters below.
     */
    // Network specific parameters
    // Get the output layers from the hailo frame.
    std::vector<NewHailoDetection> detections;
    if (tensors.size() == 0)
    {
        return detections;
    }
    // Rearrange the order of the output layers to match boxes:classes
    std::reverse(tensors.begin(), tensors.end());

    const int image_width = LIGHTFACE_WIDTH;
    const int image_height = LIGHTFACE_HEIGHT;
    const xt::xarray<float> anchor_variance = {0.1, 0.2};
    const xt::xarray<int> anchor_steps = {8, 16, 32, 64};
    const std::vector<std::vector<int>> anchor_min_size = {{10, 16, 24}, {32, 48}, {64, 96}, {128, 192, 256}};
    const int num_branches = anchor_min_size.size();
    const float score_threshold = 0.7;
    const float iou_threshold = 0.4;

    // Calculate the anchors based on the image size, step size, and feature map.
    const xt::xarray<float> anchors = get_anchors(anchor_min_size, anchor_steps, image_width, image_height);
    // Using the anchors, create a multiplier that will be used against the tensor results.
    const xt::xarray<float> anchors_multiplier = anchor_variance(0) * xt::view(anchors, xt::all(), xt::range(2, _));

    // Extract the detection objects using the given parameters.
    detections = face_detection_postprocess(tensors, anchors, anchors_multiplier, anchor_variance,
                                            score_threshold, iou_threshold, num_branches);

    return detections;
}

void lightface(HailoROIPtr roi)
{
    std::vector<NewHailoTensorPtr> tensors = roi->get_tensors();
    std::vector<NewHailoDetection> detections = lightface_post(tensors);
    hailo_common::add_detections(roi, detections);
}

//******************************************************************
//  DEFAULT FILTER
//******************************************************************
void filter(HailoROIPtr roi)
{
    lightface(roi);
}