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


// VisionX
#include <VisionX/interface/core/ImageProcessorInterface.ice>

// corcal
#include <corcal/interface/object_instance_listener.ice>
#include <corcal/interface/pointcloud_listener.ice>
#include <corcal/interface/ssr_feature_listener.ice>


module corcal { module components { module visualisation
{


interface component_interface extends
    visionx::ImageProcessorInterface,
    corcal::components::catalyst::object_instance_listener,
    corcal::components::catalyst::pointcloud_listener,
    corcal::components::ssrfeatex::ssr_feature_listener
{
    // pass
};


};};};
