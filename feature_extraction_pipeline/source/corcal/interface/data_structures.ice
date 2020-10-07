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
 * @package    corcal::core
 * @author     Christian R. G. Dreher <christian.dreher@student.kit.edu>
 * @date       2018
 * @copyright  http://www.gnu.org/licenses/gpl-2.0.txt
 *             GNU General Public License
 */


#pragma once


// Robot API
#include <RobotAPI/interface/visualization/DebugDrawerInterface.ice>

// VisionX
#include <VisionX/interface/core/DataTypes.ice>


module corcal { module core
{


sequence<int> ssr_matrix_row;
sequence<ssr_matrix_row> ssr_matrix;


struct detected_object
{
    string class_name;
    int class_index;
    string instance_name;
    float certainty;
    visionx::BoundingBox3D bounding_box;
    visionx::BoundingBox3D past_bounding_box;
    armarx::DrawColor24Bit colour;
};
sequence<detected_object> detected_object_list;


};};
