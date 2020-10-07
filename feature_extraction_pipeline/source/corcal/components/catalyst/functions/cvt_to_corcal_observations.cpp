/*
 * This file is part of ArmarX.
 *
 * ArmarX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ArmarX is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * @package    corcal::components::catalyst
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#include <corcal/components/catalyst/functions.h>


// STD/STL
#include <chrono>
#include <tuple>
#include <vector>

// IVT
#include <Image/ByteImage.h>

// VisionX
#include <VisionX/interface/components/OpenPoseEstimationInterface.h>
#include <VisionX/interface/components/YoloObjectListener.h>

// corcal
#include <corcal/core/vwm.h>


// TODO: move to core?
using namespace corcal::components::catalyst;


std::vector<corcal::core::observation::ptr>
functions::cvt_to_corcal_observations(
        const std::vector<visionx::yolo::DetectedObject>& detected_objects,
        const std::chrono::microseconds& timestamp)
{
    std::vector<corcal::core::observation::ptr> observations;
    observations.reserve(detected_objects.size());

    for (const visionx::yolo::DetectedObject& detected_object : detected_objects)
    {
        corcal::core::observation::ptr observation = std::make_shared<corcal::core::observation>();

        // Candidates
        std::vector<corcal::core::candidate> candidates;
        candidates.reserve(detected_object.candidates.size());
        for (const visionx::yolo::ClassCandidate& class_candidate : detected_object.candidates)
        {
            corcal::core::candidate candidate;
            candidate.certainty(class_candidate.certainty);
            candidate.class_index(class_candidate.classIndex);
            candidate.class_name(class_candidate.className);
            candidate.colour(class_candidate.color);
            candidates.push_back(std::move(candidate));
        }
        observation->candidates(std::move(candidates));

        // Adopt properties from VisionX data structure
        observation->class_count(detected_object.classCount);
        observation->cx(detected_object.boundingBox.x);
        observation->cy(detected_object.boundingBox.y);
        observation->h(detected_object.boundingBox.h);
        observation->w(detected_object.boundingBox.w);

        // Additional information / precomputed fields
        observation->seen_at(timestamp);
        observation->xmin(detected_object.boundingBox.x - (detected_object.boundingBox.w / 2));
        observation->xmax(detected_object.boundingBox.x + (detected_object.boundingBox.w / 2));
        observation->ymin(detected_object.boundingBox.y - (detected_object.boundingBox.h / 2));
        observation->ymax(detected_object.boundingBox.y + (detected_object.boundingBox.h / 2));

        observations.push_back(std::move(observation));
    }

    return observations;
}


std::vector<corcal::core::observation::ptr>
functions::cvt_to_corcal_observations(
        const armarx::Keypoint2DMapList& keypoint_maps,
        const std::chrono::microseconds& timestamp)
{
    std::vector<visionx::yolo::DetectedObject> hands;
    hands.reserve(keypoint_maps.size() * 2);

    // For each person
    for (const armarx::Keypoint2DMap& keypoint_map : keypoint_maps)
    {
        visionx::yolo::DetectedObject left_hand;
        visionx::yolo::DetectedObject right_hand;

        // Set candidates
        {
            visionx::yolo::ClassCandidate lcc;
            visionx::yolo::ClassCandidate rcc;

            lcc.className = "LeftHand";
            lcc.classIndex = 0;
            lcc.certainty = 1;
            lcc.color = armarx::DrawColor24Bit{199, 7, 247};
            rcc.className = "RightHand";
            rcc.classIndex = 0;
            rcc.certainty = 1;
            rcc.color = armarx::DrawColor24Bit{199, 7, 247};

            left_hand.candidates = {lcc};
            right_hand.candidates = {rcc};
        }

        left_hand.classCount = 1;
        right_hand.classCount = 1;

        // Init mins with infinity
        float rxmin = std::numeric_limits<float>::infinity();
        float rymin = std::numeric_limits<float>::infinity();
        float lxmin = std::numeric_limits<float>::infinity();
        float lymin = std::numeric_limits<float>::infinity();

        // Init max's with -infinity
        float rxmax = -std::numeric_limits<float>::infinity();
        float rymax = -std::numeric_limits<float>::infinity();
        float lxmax = -std::numeric_limits<float>::infinity();
        float lymax = -std::numeric_limits<float>::infinity();

        // For each hand keypoint (right or left)
        for (const std::pair<std::string, armarx::Keypoint2D>& keypoint_pair : keypoint_map)
        {
            const armarx::Keypoint2D& keypoint = keypoint_pair.second;

            // Skip iteration if keypoint is not valid
            if (keypoint.x == 0.0f and keypoint.y == 0.0f) continue;

            // If label starts with "LHand_", it's a keypoint of the left hand (otherwise it's "RHand_")
            if (keypoint.label.find("LHand_") == 0)
            {
                if (keypoint.x < lxmin) lxmin = keypoint.x;
                if (keypoint.x > lxmax) lxmax = keypoint.x;
                if (keypoint.y < lymin) lymin = keypoint.y;
                if (keypoint.y > lymax) lymax = keypoint.y;
            }
            else if (keypoint.label.find("RHand_") == 0)
            {
                if (keypoint.x < rxmin) rxmin = keypoint.x;
                if (keypoint.x > rxmax) rxmax = keypoint.x;
                if (keypoint.y < rymin) rymin = keypoint.y;
                if (keypoint.y > rymax) rymax = keypoint.y;
            }
        }

        if (std::isfinite(rxmin) and std::isfinite(rxmax) and std::isfinite(rymin) and std::isfinite(rymax)
                and rxmax > 0 and rymax > 0)
        {
            right_hand.boundingBox.w = rxmax - rxmin;
            right_hand.boundingBox.h = rymax - rymin;
            right_hand.boundingBox.x = rxmin + (right_hand.boundingBox.w / 2);
            right_hand.boundingBox.y = rymin + (right_hand.boundingBox.h / 2);

            hands.push_back(std::move(right_hand));
        }

        if (std::isfinite(lxmin) and std::isfinite(lxmax) and std::isfinite(lymin) and std::isfinite(lymax)
                and lxmax > 0 and lymax > 0)
        {
            left_hand.boundingBox.w = lxmax - lxmin;
            left_hand.boundingBox.h = lymax - lymin;
            left_hand.boundingBox.x = lxmin + (left_hand.boundingBox.w / 2);
            left_hand.boundingBox.y = lymin + (left_hand.boundingBox.h / 2);

            hands.push_back(std::move(left_hand));
        }
    }

    hands.shrink_to_fit();

    return functions::cvt_to_corcal_observations(std::move(hands), timestamp);
}
