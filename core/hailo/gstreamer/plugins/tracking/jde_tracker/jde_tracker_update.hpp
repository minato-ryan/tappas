/**
* Copyright (c) 2021-2022 Hailo Technologies Ltd. All rights reserved.
* Distributed under the LGPL license (https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt)
**/
#pragma once

// General cpp includes
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Tappas includes
#include "hailo_objects.hpp"
#include "kalman_filter.hpp"
#include "strack.hpp"
#include "tracker_macros.hpp"


/**
 * @brief Keep specific indices from an input vector of stracks.
 * 
 * @param stracks  -  std::vector<STrack>
 *        The stracks to keep from.
 *
 * @param indices  -  std::vector<int>
 *        The indices to keep.
 */
inline void keep_indices(std::vector<STrack> &stracks, const std::vector<int> &indices)
{
    std::vector<STrack> stracks_swap;
    stracks_swap.reserve(indices.size());
    for (uint i = 0; i < indices.size(); i++)
    {
        if ( indices[i] < (int)stracks.size() )
            stracks_swap.emplace_back(stracks[indices[i]]);
    }
    stracks = stracks_swap;
}

/**
 * @brief Keep specific indices from an input vector of stracks.
 * 
 * @param stracks  -  std::vector<STrack *>
 *        The stracks (by pointer) to keep from.
 *
 * @param indices  -  std::vector<int>
 *        The indices to keep.
 */
inline void keep_indices(std::vector<STrack *> &stracks, const std::vector<int> &indices)
{
    std::vector<STrack *> stracks_swap;
    stracks_swap.reserve(indices.size());
    for (uint i = 0; i < indices.size(); i++)
    {
        if ( indices[i] < (int)stracks.size() )
            stracks_swap.emplace_back(stracks[indices[i]]);
    }
    stracks = stracks_swap;
}

/**
 * @brief Update the matched tracklets, activating and
 *        reactivating as necessary.
 * 
 * @param matches  -  std::vector<std::pair<int,int>>
 *        Pairs of matches, generated by linear assignment.
 *
 * @param tracked_stracks  -  std::vector<STrack *>
 *        The tracked stracks (by pointer).
 *
 * @param detections  -  std::vector<STrack>
 *        The detected objects.
 *
 * @param activated_stracks  - std::vector<STrack>
 *        The currently active stracks. All matched stracks
 *        will be added here.
 */
inline void JDETracker::update_matches(std::vector<std::pair<int,int>> matches,
                                       std::vector<STrack *> tracked_stracks,
                                       std::vector<STrack> &detections,
                                       std::vector<STrack> &activated_stracks)
{
    for (uint i = 0; i < matches.size(); i++)
    {
        if ( (tracked_stracks.size() == 0) || (detections.size() == 0) )
            continue;
        STrack *track = tracked_stracks[matches[i].first];
        STrack *det = &detections[matches[i].second];
        switch(track->get_state())
        {
            case TrackState::Tracked: // The tracklet was already tracked, so update
                track->update(*det, this->m_frame_id);
                break;
            case TrackState::Lost:    // The tracklet was lost but found, so re-activate
                track->re_activate(*det, this->m_frame_id, false);
                break;
            case TrackState::New:     // The tracklet is brand new, so activate
                track->activate(this->m_kalman_filter, this->m_frame_id);
                break;
        }
        activated_stracks.push_back(*track);
    }
}

/**
 * @brief Update the tracklets that did not get matched.
 *        The state of the tracklets is updated here as needed.
 *        If a tracklet has overstayed its time in a state (keep_time),
 *        then it may be moved to another state.
 *        Example: If a tracked object has been unmatched for more than
 *                 m_keep_tracked_frames, then it will be marked lost
 *                 and moved to the list of lost_stracks
 * 
 * @param strack_pool  -  std::vector<STrack *>
 *        The pool of unmatched stracks.
 *
 * @param tracked_stracks  -  std::vector<STrack>
 *        The list of tracked stracks.
 *
 * @param lost_stracks  -  std::vector<STrack>
 *        The list of lost stracks.
 *
 * @param new_stracks  -  std::vector<STrack>
 *        The list of new stracks.
 *
 */
inline void JDETracker::update_unmatches(std::vector<STrack *> strack_pool,
                                  std::vector<STrack> &tracked_stracks,
                                  std::vector<STrack> &lost_stracks,
                                  std::vector<STrack> &new_stracks)
{
    for (uint i = 0; i < strack_pool.size(); i++)
    {
        STrack *track = strack_pool[i];
        switch(track->get_state())
        {
            case TrackState::Tracked:
                if (this->m_frame_id - track->end_frame() < this->m_keep_tracked_frames)
                {
                    tracked_stracks.push_back(*track); // Not over threshold, so still tracked
                } else {
                    track->mark_lost();
                    lost_stracks.push_back(*track);    // Over keep threshold, now lost
                }
                break;
            case TrackState::Lost:
                if (this->m_frame_id - track->end_frame() < this->m_keep_lost_frames)
                {
                    lost_stracks.push_back(*track);    // Not over threshold, so still lost
                } else {
                    track->mark_removed();             // Over keep threshold, now removed
                }
                break;
            case TrackState::New:
                if (this->m_frame_id - track->end_frame() < this->m_keep_new_frames)
                {
                    new_stracks.push_back(*track);     // Not over threshold, so still new
                } else {
                    track->mark_removed();             // Over keep threshold, now removed
                }
                break;
        }
    }
}

/**
 * @brief The main logic unit and access point of the tracker system. 
 *        On each new frame of the pipeline, this function should be called
 *        and given the new detections in the frame. The following steps
 *        take place:
 *        Step 1: New detections are converted into stracks. Then a kalman filter predicts
 *                the possible motion of the current tracked objects.
 *        Step 2: Matches are made between tracked objects and detections
 *                based on gating (Mahalanobis) distances.
 *                Matched stracks are updated/activated/re-activated as needed.
 *        Step 3: Matches are made between the leftover tracked and detected objects
 *                based on iou distances.
 *                Matched stracks are updated/activated/re-activated as needed.
 *        Step 4: Matches are made between leftover detections and unconfirmed (new)
 *                objects from the previous frames. Matches are based on iou
 *                distances, but with a lower threshold.
 *                Matched stracks are updated/activated/re-activated as needed.
 *        Step 5: Any leftover new detections are added as new objects to track.
 *        Step 6: Tracker database is updated.
 *        Step 7: Outputs are chosen from the tracked objects.
 * 
 * @param inputs  -  std::vector<NewHailoDetectionPtr>
 *        The new detections from this frame.
 *
 * @param report_unconfirmed  -  bool
 *        If true, then output unconfirmed stracks as well.
 *
 * @return std::vector<STrack>
 *         The currently tracked (and unconfirmed if report_unconfirmed) objects.
 */
inline std::vector<STrack> JDETracker::update(std::vector<NewHailoDetectionPtr> &inputs, bool report_unconfirmed = false)
{
    this->m_frame_id++;
    std::vector<STrack> detections;            // New detections in this update
    std::vector<STrack> activated_stracks;     // Currently active stracks
    std::vector<STrack> lost_stracks;          // Currently lost stracks
    std::vector<STrack> new_stracks;           // Currently new stracks

    std::vector<STrack *> strack_pool;         // A pool of tracked/lost stracks to find matches for

    std::vector<std::vector<float>> distances; // A distance cost matrix for linear assignment
    std::vector<std::pair<int,int>> matches;   // Pairs of matches between sets of stracks
    std::vector<int> unmatched_tracked;        // Unmatched tracked stracks
    std::vector<int> unmatched_detections;     // Unmatched new detections

    //******************************************************************
    // Step 1: Prepare tracks for new detections
    //******************************************************************
    detections = JDETracker::hailo_detections_to_stracks(inputs);  // Convert the new detections into STracks

    strack_pool = joint_strack_pointers(this->m_tracked_stracks, this->m_lost_stracks); // Pool together the tracked and lost stracks
    STrack::multi_predict(strack_pool, this->m_kalman_filter);                          // Run Kalman Filter prediction step

    //******************************************************************
    // Step 2: First association, tracked with embedding
    //******************************************************************
    // Calculate the distances between the tracked/lost stracks and the newly detected inputs
    embedding_distance(strack_pool, detections, distances); // Calculate the distances
    fuse_motion(distances, strack_pool, detections);        // Create the cost matrix

    // Use linear assignment to find matches
    linear_assignment(distances, strack_pool.size(), detections.size(), this->m_kalman_dist_thr, matches, unmatched_tracked, unmatched_detections);

    // Update the matches
    update_matches(matches, strack_pool, detections, activated_stracks);

    //******************************************************************
    // Step 3: Second association, leftover tracked with IOU
    //******************************************************************
    // Use the unmatched_detections indices to get a vector of just the unmatched new detections
    keep_indices(detections, unmatched_detections);

    // Use the unmatched_tracked indices to get a vector of only unmatched, previously tracked, but-not-yet-lost stracks
    keep_indices(strack_pool, unmatched_tracked);

    // Instead of embedding distance, this time we will associate based on iou,
    // so calculate the iou distance of what's left
    distances = iou_distance(strack_pool, detections);

    // Recalculate the linear assignment, this time use the iou threshold
    linear_assignment(distances, strack_pool.size(), detections.size(), this->m_iou_thr, matches, unmatched_tracked, unmatched_detections);

    // Update the matches
    update_matches(matches, strack_pool, detections, activated_stracks);

    // Break down the strack_pool to just the remaining unmatched stracks
    keep_indices(strack_pool, unmatched_tracked);

    // Update the state of the remaining unmatched stracks
    update_unmatches(strack_pool, activated_stracks, lost_stracks, new_stracks);

    //******************************************************************
    // Step 4: Third association, uncomfirmed with weaker IOU
    //******************************************************************
    // Deal with the unconfirmed stracks, these are usually stracks with only one beginning frame
    // Use the unmatched_detections indices to get a vector of just the unmatched new detections again
    keep_indices(detections, unmatched_detections);
    std::vector<STrack> blank;
    std::vector<STrack *> unconfirmed_pool = joint_strack_pointers(this->m_new_stracks, blank);  // Prepare a pool of unconfirmed stracks

    // Recalculate the iou distance, this time between unconfirmed stracks and the remaining detections
	distances = iou_distance(unconfirmed_pool, detections);

    // Recalculate the linear assignment, this time with the lower m_init_iou_thr threshold
    linear_assignment(distances, unconfirmed_pool.size(), detections.size(), this->m_init_iou_thr, matches, unmatched_tracked, unmatched_detections);

    // Update the matches
    update_matches(matches, unconfirmed_pool, detections, activated_stracks);

    // Break down the strack_pool to just the remaining unmatched stracks
    keep_indices(unconfirmed_pool, unmatched_tracked);

    // Update the state of the remaining unmatched stracks
    update_unmatches(unconfirmed_pool, activated_stracks, lost_stracks, new_stracks);

    //******************************************************************
    // Step 5: Init new stracks
    //******************************************************************
    // At this point, any leftover unmatched new detections are considered new object instances for tracking
    for (uint i = 0; i < unmatched_detections.size(); i++)
        new_stracks.emplace_back(detections[unmatched_detections[i]]);

    //******************************************************************
    // Step 6: Update Database
    //******************************************************************
    // Update the tracker database members with the results of this update
    this->m_tracked_stracks = activated_stracks;
    this->m_lost_stracks = lost_stracks;
    this->m_new_stracks = new_stracks;

    //******************************************************************
    // Step 7: Set the output stracks
    //******************************************************************
    std::vector<STrack> output_stracks;
    output_stracks.reserve(this->m_tracked_stracks.size());
    for (uint i = 0; i < this->m_tracked_stracks.size(); i++)
        output_stracks.emplace_back(this->m_tracked_stracks[i]);

    // Include unconfirmed detections if requested
    if (report_unconfirmed)
    {
        for (uint i = 0; i < this->m_new_stracks.size(); i++)
            output_stracks.emplace_back(this->m_new_stracks[i]);
    }

    return output_stracks;
}