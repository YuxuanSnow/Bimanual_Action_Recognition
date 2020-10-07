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
 * @package    corcal::components::cnnreplay
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2019
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#pragma once


// STD/STL
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>

// VisionX
#include <VisionX/libraries/playback.h>
#include <VisionX/core/CapturingImageProvider.h>
#include <VisionX/interface/components/OpenPoseEstimationInterface.h>
#include <VisionX/interface/components/YoloObjectListener.h>

// corcal
#include <corcal/interface/catalyst_component_interface.h>
#include <corcal/interface/cnnreplay_component_interface.h>


namespace corcal::components::cnnreplay
{


class component :
    virtual public component_interface,
    virtual public visionx::CapturingImageProvider
{

public:

    static const std::string default_name;

protected:

    const long long m_timestamp_offset = 1000000000;
    long long m_frame_offset = 0;

    std::mutex m_proc_mutex;
    std::condition_variable m_proc_signal;
    bool m_shutdown;
    bool m_received_3d_objects;
    bool m_received_ssr_feats;

    unsigned int m_current_rec_index;
    unsigned int m_current_frame_index;
    std::vector<std::filesystem::path> m_rec_paths;
    std::vector<visionx::playback::Playback> playbacks;

    // Topics
    visionx::yolo::ObjectListener::ProxyType m_objects_topic;
    armarx::OpenPose2DListener::ProxyType m_pose_body_topic;
    armarx::OpenPoseHand2DListener::ProxyType m_pose_hand_topic;

    catalyst::component_interface::ProxyType m_catalyst;
    bool m_catalyst_initialised;

public:

    component();
    virtual ~component() override;

    virtual void onInitCapturingImageProvider() override;
    virtual void onExitCapturingImageProvider() override;
    virtual void onStartCapture(float) override;
    virtual void onStopCapture() override;
    virtual bool capture(void** image_buffer) override;

    virtual std::string getDefaultName() const override;
    virtual armarx::PropertyDefinitionsPtr createPropertyDefinitions() override;

    void
    virtual object_instances_detected(
        const std::vector<core::detected_object>& objects,
        Ice::Long timestamp_darknet,
        Ice::Long timestamp_openpose,
        const Ice::Current&
    ) override;

    virtual void ssr_features_detected(
        const std::vector<corcal::core::detected_object>& objects,
        const std::vector<std::vector<int>>& ssr_matrix,
        Ice::Long timestamp,
        const Ice::Current&
    ) override;

private:

    enum class path_type
    {
        depth,
        rgb,
        objects_2d,
        objects_3d,
        body_pose,
        hand_pose,
        spatial_relations
    };

    std::filesystem::path path_type_to_name(path_type) const;

    std::filesystem::path get_path(path_type) const;
    visionx::yolo::DetectedObjectList get_objects() const;
    armarx::Keypoint2DMapList get_pose_body() const;
    armarx::Keypoint2DMapList get_pose_hand() const;

    /**
     * @brief Publishes the current table angle (relative to the camera) and its height
     */
    void table_hack();

};


}
