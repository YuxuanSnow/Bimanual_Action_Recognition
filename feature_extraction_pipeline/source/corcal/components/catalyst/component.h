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


#pragma once


// STD/STL
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// IVT
#include <Image/ByteImage.h>

// ArmarX
#include <ArmarXCore/core/services/tasks/RunningTask.h>

// VisionX
#include <VisionX/core/ImageProcessor.h>
#include <VisionX/interface/components/YoloObjectListener.h>

// corcal
#include <corcal/core/vwm.h>
#include <corcal/interface/catalyst_component_interface.h>
#include <corcal/interface/object_instance_listener.h>
#include <corcal/interface/pointcloud_listener.h>


namespace corcal::components::catalyst
{


class component :
    virtual public visionx::ImageProcessor,
    virtual public component_interface
{

    public:

        static const std::string default_name;

    private:

        // IPC
        std::string m_image_provider_id;
        visionx::ImageProviderInfo m_image_provider_info;
        object_instance_listenerPrx m_object_instance_listener;
        pointcloud_listenerPrx m_pointcloud_listener;
        bool m_ignore_cnns = false;

        // Table hack variables
        mutable std::mutex m_table_hack_mutex;
        double m_table_angle;
        double m_table_offset_rl;
        double m_table_offset_h;
        double m_table_offset_d;

        bool m_use_manual_timestamps;

        // Mutexes and synchronisation
        std::mutex m_input_proc_mutex;
        std::condition_variable m_proc_signal;
        armarx::RunningTask<component>::pointer_type m_input_synchronisation_task;

        // Input data buffers
        ::CByteImage** m_input_image_buffer;
        std::vector<armarx::Keypoint2DMap> m_body_pose_buffer;
        std::vector<armarx::Keypoint2DMap> m_hand_pose_buffer;
        std::map<std::chrono::microseconds, ::CByteImage**> m_long_term_image_buffer;
        unsigned int m_long_term_image_buffer_max_size;
        corcal::core::memory m_memory;

        // Timestamps
        std::chrono::microseconds m_timestamp_last_input_image;
        std::chrono::microseconds m_timestamp_last_detected_objects;
        std::chrono::microseconds m_timestamp_last_body_pose;
        std::chrono::microseconds m_timestamp_last_hand_pose;

    public:

        virtual ~component() override;

        std::string
        virtual getDefaultName() const override;

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
            const armarx::Keypoint2DMapList&,
            Ice::Long,
            const Ice::Current&
        ) override { /* pass */ }

        void
        virtual report2DHandKeypoints(
            const armarx::Keypoint2DMapList&,
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

        void
        virtual table_location_hack(
            double angle,
            double offset_rl,
            double offset_h,
            double offset_d,
            const Ice::Current&
        ) override;

        std::vector<corcal::core::detected_object>
        virtual get_objects_from_file();

        void
        virtual reset_memory(const Ice::Current&) override;

        void
        virtual use_manual_timestamps(bool enable, const Ice::Current&) override;

    protected:

        void
        virtual onInitImageProcessor() override;

        void
        virtual onConnectImageProcessor() override;

        void
        virtual onDisconnectImageProcessor() override;

        void
        virtual onExitImageProcessor() override;

        void
        virtual run_input_synchronisation();

        void
        virtual process() override;

        ::CByteImage**
        get_closest_image(const std::chrono::microseconds& timestamp) const;

        std::vector<corcal::core::detected_object>
        process_inputs(
            const std::vector<corcal::core::known_object::ptr>& objects,
            ::CByteImage** input_image_detected_objects,
            const std::chrono::microseconds& detected_objects_timestamp,
            ::CByteImage** input_image_hand_pose,
            const std::chrono::microseconds& hand_pose_timestamp,
            const std::chrono::milliseconds& bounding_box_smoothing
        ) const;

        armarx::PropertyDefinitionsPtr
        virtual createPropertyDefinitions() override;

};


}
