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
 * @package    corcal::components::visualisation
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#pragma once


// STD/STL
#include <chrono>
#include <string>

// RobotAPI
#include <RobotAPI/interface/visualization/DebugDrawerInterface.h>

// VisionX
#include <VisionX/core/ImageProcessor.h>

// corcal
#include <corcal/interface/data_structures.h>
#include <corcal/interface/visualisation_component_interface.h>


namespace corcal::components::visualisation
{


class component :
    virtual public component_interface,
    virtual public visionx::ImageProcessor
{

    public:

        static const std::string default_name;

    private:

        // IPC
        std::string m_image_provider_id;
        visionx::ImageProviderInfo m_image_provider_info;

        // Mutexes and synchronisation
        std::mutex m_input_image_mutex;

        // Input data buffers
        ::CByteImage** m_input_image_buf;
        std::map<std::chrono::microseconds, ::CByteImage*> m_rgb_image_buffer;
        unsigned int m_rgb_image_buffer_max_size;

        armarx::DebugDrawerInterfacePrx m_debug_drawer;

        unsigned int m_draw_layer;
        unsigned int m_draw_layer2;

    public:

        virtual ~component() override;

        std::string
        virtual getDefaultName() const override;

        void
        virtual pointcloud_generated(
            const armarx::DebugDrawerPointCloud& pointcloud,
            Ice::Long timestamp,
            const Ice::Current&
        ) override;

        void
        virtual object_instances_detected(
                const std::vector<corcal::core::detected_object>& objects,
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
        virtual process() override;

        armarx::PropertyDefinitionsPtr
        virtual createPropertyDefinitions() override;

};


}
