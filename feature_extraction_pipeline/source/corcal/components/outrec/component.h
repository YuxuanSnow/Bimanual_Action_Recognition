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
 * @package    corcal::components::outrec
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

// ArmarX
#include <ArmarXCore/core/services/tasks/RunningTask.h>

// VisionX
#include <VisionX/libraries/playback.h>
#include <VisionX/core/ImageProvider.h>

// corcal
#include <corcal/interface/outrec_component_interface.h>


namespace corcal::components::outrec
{


class component :
    virtual public component_interface,
    virtual public visionx::ImageProvider
{

public:

    static const std::string default_name;

protected:

    std::mutex m_proc_mutex;
    std::condition_variable m_proc_signal;
    bool m_received_darknet_objects;
    bool m_received_openpose_body_pose;
    bool m_received_openpose_hand_pose;
    unsigned int m_current_rec_index;
    unsigned int m_current_frame_index;
    std::vector<std::filesystem::path> m_rec_paths;
    std::vector<visionx::playback::Playback> playbacks;
    armarx::RunningTask<component>::pointer_type m_input_synchronisation_task;

public:

    component();
    virtual ~component() override;

    virtual void onInitImageProvider() override;
    virtual void onConnectImageProvider() override;
    virtual void onDisconnectImageProvider() override;
    virtual void onExitImageProvider() override;

    virtual void provide_images();

    virtual std::string getDefaultName() const override;
    virtual armarx::PropertyDefinitionsPtr createPropertyDefinitions() override;

    void
    virtual announceDetectedObjects(
        Ice::Long, const Ice::Current&
    ) override { /* pass */ }

    void
    virtual reportDetectedObjects(
        const std::vector<visionx::yolo::DetectedObject>& dol,
        Ice::Long,
        const Ice::Current&
    ) override;

    void
    virtual report2DKeypoints(
        const armarx::Keypoint2DMapList& m_body_pose,
        Ice::Long,
        const Ice::Current&
    ) override { /* pass */ }

    void
    virtual report2DHandKeypoints(
        const armarx::Keypoint2DMapList& m_body_pose,
        Ice::Long,
        const Ice::Current&
    ) override { /* pass */ }

    void
    virtual report2DHandKeypointsNormalized(
        const armarx::Keypoint2DMapList&,
        Ice::Long,
        const Ice::Current&
    ) override;

    void
    virtual report2DKeypointsNormalized(
        const armarx::Keypoint2DMapList& m_hand_pose,
        Ice::Long timestamp,
        const Ice::Current&
    ) override;

private:

    std::filesystem::path get_path(const std::string& kind);

};


}
